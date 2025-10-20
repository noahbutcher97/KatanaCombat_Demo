"""
File system collector for UE5 projects.
Browses project files, reads source code, and collects file context.
"""
import os
from pathlib import Path
from typing import TYPE_CHECKING, Any, Dict, Optional

if TYPE_CHECKING:
    import unreal  # type: ignore

try:
    import unreal  # type: ignore
    UNREAL_AVAILABLE = True
except ImportError:
    UNREAL_AVAILABLE = False
    print(
        "[FileCollector] Unreal module not available - "
        "running in standalone mode"
    )


class FileCollector:
    """Collects file system information from UE5 projects."""
    
    def __init__(self):
        """Initialize file collector using UE5.6 Paths API."""
        if UNREAL_AVAILABLE:
            # Use official UE5.6 Paths API
            self.project_dir = unreal.Paths.project_dir()  # type: ignore
            self.content_dir = unreal.Paths.project_content_dir()  # type: ignore
            self.source_dir = os.path.join(self.project_dir, "Source")
            self.saved_dir = unreal.Paths.project_saved_dir()  # type: ignore
            self.config_dir = unreal.Paths.project_config_dir()  # type: ignore
        else:
            self.project_dir = os.getcwd()
            self.content_dir = os.path.join(self.project_dir, "Content")
            self.source_dir = os.path.join(self.project_dir, "Source")
            self.saved_dir = os.path.join(self.project_dir, "Saved")
            self.config_dir = os.path.join(self.project_dir, "Config")
    
    def list_directory(
        self,
        directory_path: Optional[str] = None,
        recursive: bool = False,
        max_depth: int = 5
    ) -> Dict[str, Any]:
        """
        List files in a directory.
        
        Args:
            directory_path: Path to list. If None, uses project directory.
            recursive: Whether to list recursively.
            max_depth: Maximum recursion depth.
            
        Returns:
            Dictionary with file context including file list.
        """
        if directory_path is None:
            directory_path = self.project_dir
        
        path = Path(directory_path)
        if not path.exists():
            return {
                "root_path": str(path),
                "files": [],
                "total_files": 0,
                "total_size": 0,
                "error": f"Path not found: {directory_path}"
            }
        
        files = []
        total_size = 0
        
        if recursive:
            for root, _, filenames in os.walk(path):
                depth = len(Path(root).relative_to(path).parts)
                if depth > max_depth:
                    continue
                
                for filename in filenames:
                    file_path = Path(root) / filename
                    file_entry = self._get_file_entry(file_path)
                    files.append(file_entry)
                    total_size += file_entry.get("size", 0) or 0
        else:
            for item in path.iterdir():
                file_entry = self._get_file_entry(item)
                files.append(file_entry)
                if file_entry["type"] == "file":
                    total_size += file_entry.get("size", 0) or 0
        
        return {
            "root_path": str(path),
            "files": files,
            "total_files": len(files),
            "total_size": total_size
        }
    
    def read_file(self, file_path: str) -> Dict[str, Any]:
        """
        Read a file's content.
        
        Args:
            file_path: Path to the file.
            
        Returns:
            File entry with content.
        """
        path = Path(file_path)
        
        if not path.exists():
            return {
                "path": file_path,
                "name": path.name,
                "type": "file",
                "error": f"File not found: {file_path}"
            }
        
        if not path.is_file():
            return {
                "path": file_path,
                "name": path.name,
                "type": "directory",
                "error": "Not a file"
            }
        
        file_entry = self._get_file_entry(path, include_content=True)
        return file_entry
    
    def search_files(
        self,
        root_path: Optional[str] = None,
        pattern: Optional[str] = None,
        extension: Optional[str] = None,
        max_results: int = 100
    ) -> Dict[str, Any]:
        """
        Search for files matching criteria.
        
        Args:
            root_path: Root path to search. If None, uses project directory.
            pattern: Filename pattern to match (case-insensitive).
            extension: File extension to filter (e.g., '.cpp', '.h').
            max_results: Maximum number of results.
            
        Returns:
            File context with matching files.
        """
        if root_path is None:
            root_path = self.project_dir
        
        path = Path(root_path)
        if not path.exists():
            return {
                "root_path": str(path),
                "files": [],
                "total_files": 0,
                "total_size": 0,
                "error": f"Path not found: {root_path}"
            }
        
        files = []
        total_size = 0
        
        for root, _, filenames in os.walk(path):
            for filename in filenames:
                if len(files) >= max_results:
                    break
                
                if pattern and pattern.lower() not in filename.lower():
                    continue
                
                if extension and not filename.endswith(extension):
                    continue
                
                file_path = Path(root) / filename
                file_entry = self._get_file_entry(file_path)
                files.append(file_entry)
                total_size += file_entry.get("size", 0) or 0
        
        search_query = []
        if pattern:
            search_query.append(f"pattern={pattern}")
        if extension:
            search_query.append(f"ext={extension}")
        
        return {
            "root_path": str(path),
            "files": files,
            "search_query": ", ".join(search_query) if search_query else None,
            "total_files": len(files),
            "total_size": total_size
        }
    
    def get_source_files(self, max_files: int = 50) -> Dict[str, Any]:
        """
        Get C++ source files from the project.
        
        Args:
            max_files: Maximum number of files to return.
            
        Returns:
            File context with C++ source files.
        """
        if not os.path.exists(self.source_dir):
            return {
                "root_path": self.source_dir,
                "files": [],
                "total_files": 0,
                "total_size": 0,
                "error": "No Source directory found"
            }
        
        cpp_files = []
        h_files = []
        total_size = 0
        
        for root, _, filenames in os.walk(self.source_dir):
            for filename in filenames:
                if filename.endswith('.cpp'):
                    file_path = Path(root) / filename
                    file_entry = self._get_file_entry(file_path)
                    cpp_files.append(file_entry)
                    total_size += file_entry.get("size", 0) or 0
                elif filename.endswith('.h'):
                    file_path = Path(root) / filename
                    file_entry = self._get_file_entry(file_path)
                    h_files.append(file_entry)
                    total_size += file_entry.get("size", 0) or 0
                
                if len(cpp_files) + len(h_files) >= max_files:
                    break
        
        all_files = h_files + cpp_files
        
        return {
            "root_path": self.source_dir,
            "files": all_files[:max_files],
            "total_files": len(all_files),
            "total_size": total_size,
            "file_types": {
                "headers": len(h_files),
                "cpp": len(cpp_files)
            }
        }
    
    def _get_file_entry(
        self, path: Path, include_content: bool = False
    ) -> Dict[str, Any]:
        """Convert a path to a file entry dictionary."""
        try:
            stat = path.stat()
            entry = {
                "path": str(path),
                "name": path.name,
                "type": "directory" if path.is_dir() else "file",
                "extension": path.suffix if path.is_file() else None,
                "size": stat.st_size if path.is_file() else None,
                "modified": stat.st_mtime
            }
            
            if include_content and path.is_file():
                try:
                    with open(path, 'r', encoding='utf-8') as f:
                        entry["content"] = f.read()
                except Exception as e:
                    entry["content"] = None
                    entry["read_error"] = str(e)
            
            return entry
        except Exception as e:
            return {
                "path": str(path),
                "name": path.name,
                "type": "unknown",
                "error": str(e)
            }
