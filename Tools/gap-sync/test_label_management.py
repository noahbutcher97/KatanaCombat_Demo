#!/usr/bin/env python3
"""
Test script for label management functionality in sync_gaps.py

This script validates:
1. Label existence checking
2. Label creation via GitHub API
3. Pre-checking all labels before processing
4. Error handling for label operations
5. Color assignment based on label type

Usage:
    python3 test_label_management.py [--with-api]
    
Options:
    --with-api: Actually test with GitHub API (requires valid token)
"""

import sys
import os
import unittest
from unittest.mock import Mock, patch, MagicMock
import json

# Add current directory to path
sys.path.insert(0, os.path.dirname(__file__))

import sync_gaps


class TestLabelManagement(unittest.TestCase):
    """Test label management functions."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.test_owner = "test-owner"
        self.test_repo = "test-repo"
        self.test_token = "test_token_12345"
    
    def test_get_repository_info_success(self):
        """Test successful repository info retrieval."""
        mock_output = json.dumps({
            "owner": {"login": "test-owner"},
            "name": "test-repo"
        })
        
        with patch('subprocess.run') as mock_run:
            mock_run.return_value = Mock(
                returncode=0,
                stdout=mock_output,
                stderr=""
            )
            
            owner, repo = sync_gaps.get_repository_info()
            
            self.assertEqual(owner, "test-owner")
            self.assertEqual(repo, "test-repo")
    
    def test_get_repository_info_failure(self):
        """Test repository info retrieval failure."""
        with patch('subprocess.run') as mock_run:
            mock_run.return_value = Mock(
                returncode=1,
                stdout="",
                stderr="Not a git repository"
            )
            
            owner, repo = sync_gaps.get_repository_info()
            
            self.assertIsNone(owner)
            self.assertIsNone(repo)
    
    def test_ensure_label_exists_already_exists(self):
        """Test label checking when label already exists."""
        mock_labels = [
            {"name": "gap", "color": "5319E7"},
            {"name": "priority: p1", "color": "D73A4A"}
        ]
        
        with patch('requests.get') as mock_get:
            mock_get.return_value = Mock(
                status_code=200,
                json=lambda: mock_labels
            )
            
            result = sync_gaps.ensure_label_exists(
                self.test_owner,
                self.test_repo,
                "gap",
                self.test_token
            )
            
            self.assertTrue(result)
            # Should only call GET, not POST
            mock_get.assert_called_once()
    
    def test_ensure_label_exists_creates_new(self):
        """Test label creation when label doesn't exist."""
        mock_labels = [
            {"name": "other-label", "color": "000000"}
        ]
        
        with patch('requests.get') as mock_get, \
             patch('requests.post') as mock_post:
            
            mock_get.return_value = Mock(
                status_code=200,
                json=lambda: mock_labels
            )
            
            mock_post.return_value = Mock(
                status_code=201,
                text="Created"
            )
            
            result = sync_gaps.ensure_label_exists(
                self.test_owner,
                self.test_repo,
                "gap",
                self.test_token
            )
            
            self.assertTrue(result)
            mock_get.assert_called_once()
            mock_post.assert_called_once()
            
            # Verify the POST payload
            call_args = mock_post.call_args
            payload = call_args[1]['json']
            self.assertEqual(payload['name'], "gap")
            self.assertEqual(payload['color'], "5319E7")  # Purple for 'gap'
    
    def test_ensure_label_color_assignment(self):
        """Test correct color assignment based on label type."""
        test_cases = [
            ("priority: p1", "D73A4A"),  # Red
            ("status: pending", "0E8A16"),  # Green
            ("area: animation", "FBCA04"),  # Yellow
            ("type: bug", "FBCA04"),  # Yellow
            ("gap", "5319E7"),  # Purple
            ("system: paired-animation", "1D76DB"),  # Blue
            ("custom-label", "6D9EEB"),  # Default blue
        ]
        
        mock_labels = []  # Empty list means all labels need creation
        
        for label_name, expected_color in test_cases:
            with patch('requests.get') as mock_get, \
                 patch('requests.post') as mock_post:
                
                mock_get.return_value = Mock(
                    status_code=200,
                    json=lambda: mock_labels
                )
                
                mock_post.return_value = Mock(
                    status_code=201,
                    text="Created"
                )
                
                result = sync_gaps.ensure_label_exists(
                    self.test_owner,
                    self.test_repo,
                    label_name,
                    self.test_token
                )
                
                self.assertTrue(result, f"Failed for label: {label_name}")
                
                # Verify the color
                call_args = mock_post.call_args
                payload = call_args[1]['json']
                self.assertEqual(
                    payload['color'],
                    expected_color,
                    f"Wrong color for {label_name}: expected {expected_color}, got {payload['color']}"
                )
    
    def test_ensure_label_exists_api_error(self):
        """Test error handling when API call fails."""
        with patch('requests.get') as mock_get:
            mock_get.return_value = Mock(
                status_code=403,
                text="Forbidden"
            )
            
            result = sync_gaps.ensure_label_exists(
                self.test_owner,
                self.test_repo,
                "gap",
                self.test_token
            )
            
            self.assertFalse(result)
    
    def test_ensure_label_exists_creation_fails(self):
        """Test handling when label creation fails."""
        mock_labels = []  # Label doesn't exist
        
        with patch('requests.get') as mock_get, \
             patch('requests.post') as mock_post:
            
            mock_get.return_value = Mock(
                status_code=200,
                json=lambda: mock_labels
            )
            
            mock_post.return_value = Mock(
                status_code=422,
                text="Validation failed"
            )
            
            result = sync_gaps.ensure_label_exists(
                self.test_owner,
                self.test_repo,
                "gap",
                self.test_token
            )
            
            self.assertFalse(result)
    
    def test_ensure_label_exists_network_error(self):
        """Test handling of network errors."""
        import requests.exceptions
        
        with patch('requests.get') as mock_get:
            mock_get.side_effect = requests.exceptions.Timeout()
            
            result = sync_gaps.ensure_label_exists(
                self.test_owner,
                self.test_repo,
                "gap",
                self.test_token
            )
            
            self.assertFalse(result)
    
    def test_ensure_all_labels_exist_success(self):
        """Test pre-checking all labels for a set of gaps."""
        # Create some test gaps
        gaps = [
            sync_gaps.Gap(
                gap_id="1.1",
                description="Test gap 1",
                priority="P1",
                status="Pending",
                category="Animation",
                category_num=1
            ),
            sync_gaps.Gap(
                gap_id="2.1",
                description="Test gap 2",
                priority="P2",
                status="Partial",
                category="Combat",
                category_num=2
            )
        ]
        
        with patch('sync_gaps.get_repository_info') as mock_repo, \
             patch('sync_gaps.ensure_label_exists') as mock_ensure:
            
            mock_repo.return_value = (self.test_owner, self.test_repo)
            mock_ensure.return_value = True
            
            result = sync_gaps.ensure_all_labels_exist(gaps, self.test_token)
            
            self.assertTrue(result)
            # Should have called ensure_label_exists for each unique label
            self.assertGreater(mock_ensure.call_count, 0)
    
    def test_ensure_all_labels_exist_no_repo_info(self):
        """Test failure when repository info cannot be retrieved."""
        gaps = [
            sync_gaps.Gap(
                gap_id="1.1",
                description="Test gap",
                priority="P1",
                status="Pending",
                category="Animation",
                category_num=1
            )
        ]
        
        with patch('sync_gaps.get_repository_info') as mock_repo:
            mock_repo.return_value = (None, None)
            
            result = sync_gaps.ensure_all_labels_exist(gaps, self.test_token)
            
            self.assertFalse(result)
    
    def test_ensure_all_labels_exist_some_fail(self):
        """Test handling when some labels fail to be created."""
        gaps = [
            sync_gaps.Gap(
                gap_id="1.1",
                description="Test gap",
                priority="P1",
                status="Pending",
                category="Animation",
                category_num=1
            )
        ]
        
        with patch('sync_gaps.get_repository_info') as mock_repo, \
             patch('sync_gaps.ensure_label_exists') as mock_ensure:
            
            mock_repo.return_value = (self.test_owner, self.test_repo)
            # Simulate one label failing
            mock_ensure.side_effect = [True, True, False, True]
            
            result = sync_gaps.ensure_all_labels_exist(gaps, self.test_token)
            
            self.assertFalse(result)


class TestLabelManagementWithAPI(unittest.TestCase):
    """Integration tests that actually call GitHub API (requires valid token)."""
    
    @classmethod
    def setUpClass(cls):
        """Check if API tests should run."""
        cls.run_api_tests = '--with-api' in sys.argv
        if cls.run_api_tests:
            cls.token = os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
            if not cls.token:
                print("WARNING: --with-api specified but no token found")
                cls.run_api_tests = False
    
    def setUp(self):
        """Skip if API tests disabled."""
        if not self.run_api_tests:
            self.skipTest("API tests not enabled (use --with-api flag)")
    
    def test_get_repository_info_real(self):
        """Test actual repository info retrieval."""
        owner, repo = sync_gaps.get_repository_info()
        
        self.assertIsNotNone(owner)
        self.assertIsNotNone(repo)
        print(f"✅ Repository: {owner}/{repo}")
    
    def test_ensure_label_exists_real(self):
        """Test actual label checking/creation."""
        owner, repo = sync_gaps.get_repository_info()
        if not owner or not repo:
            self.skipTest("Could not get repository info")
        
        # Test with a common label that likely exists
        result = sync_gaps.ensure_label_exists(
            owner,
            repo,
            "gap",
            self.token
        )
        
        self.assertTrue(result)
        print("✅ Label 'gap' verified/created")


def main():
    """Run all tests."""
    # Remove --with-api from sys.argv before unittest sees it
    if '--with-api' in sys.argv:
        sys.argv.remove('--with-api')
    
    # Run tests
    unittest.main(verbosity=2)


if __name__ == '__main__':
    main()
