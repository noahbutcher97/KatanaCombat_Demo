"""
Async API client for communicating with the FastAPI backend.
Handles HTTP requests without blocking the Unreal Editor.
"""
import json
import time
from typing import Any, Callable, Dict, Optional

try:
    import requests
except ImportError:
    requests = None  # type: ignore

from ..core.config import get_config
from ..core.utils import Logger


class APIClient:
    """Handles async communication with the FastAPI backend."""

    def __init__(self):
        self.config = get_config()
        self.logger = Logger("APIClient", self.config.verbose)

    def post_json(
        self,
        url: str,
        payload: Dict[str, Any],
        timeout: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Send POST request with JSON payload.
        Includes retry logic and error handling.
        """
        if requests is None:
            self.logger.error("requests library not available")
            return {"error": "requests library not installed"}

        if timeout is None:
            timeout = self.config.get("timeout", 25)

        max_retries = self.config.get("max_retries", 3)
        retry_delay = self.config.get("retry_delay", 2.5)

        last_error = None
        
        # Build full URL if relative path provided
        full_url = url
        if url.startswith('/'):
            base_url = self.config.api_url
            full_url = f"{base_url}{url}"

        for attempt in range(1, max_retries + 1):
            try:
                self.logger.debug(
                    f"POST {full_url} (attempt {attempt}/{max_retries})"
                )

                response = requests.post(
                    full_url, json=payload, timeout=timeout
                )
                response.raise_for_status()

                result = response.json()
                self.logger.debug(f"Success: {len(str(result))} bytes")
                return result

            except requests.Timeout:
                last_error = f"Request timed out after {timeout}s"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except requests.HTTPError as e:
                status = e.response.status_code
                last_error = f"HTTP {status}: {e.response.text[:200]}"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except requests.RequestException as e:
                last_error = str(e)
                self.logger.warn(
                    f"Request failed: {last_error} "
                    f"(attempt {attempt}/{max_retries})"
                )

            except json.JSONDecodeError:
                last_error = "Invalid JSON response from server"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except Exception as e:
                last_error = f"Unexpected error: {e}"
                self.logger.error(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            if attempt < max_retries:
                self.logger.info(
                    f"Retrying in {retry_delay}s..."
                )
                time.sleep(retry_delay)

        self.logger.error(
            f"All {max_retries} attempts failed. Last error: "
            f"{last_error}"
        )
        return {"error": last_error or "Unknown error"}

    def get_json(
        self,
        url: str,
        params: Optional[Dict[str, Any]] = None,
        timeout: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Send GET request and return JSON response.
        Includes retry logic and error handling.
        """
        if requests is None:
            self.logger.error("requests library not available")
            return {"error": "requests library not installed"}

        if timeout is None:
            timeout = self.config.get("timeout", 25)

        max_retries = self.config.get("max_retries", 3)
        retry_delay = self.config.get("retry_delay", 2.5)

        last_error = None

        for attempt in range(1, max_retries + 1):
            try:
                self.logger.debug(
                    f"GET {url} (attempt {attempt}/{max_retries})"
                )

                response = requests.get(
                    url, params=params, timeout=timeout
                )
                response.raise_for_status()

                result = response.json()
                self.logger.debug(f"Success: {len(str(result))} bytes")
                return result

            except requests.Timeout:
                last_error = f"Request timed out after {timeout}s"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except requests.HTTPError as e:
                status = e.response.status_code
                last_error = f"HTTP {status}: {e.response.text[:200]}"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except requests.RequestException as e:
                last_error = str(e)
                self.logger.warn(
                    f"Request failed: {last_error} "
                    f"(attempt {attempt}/{max_retries})"
                )

            except json.JSONDecodeError:
                last_error = "Invalid JSON response from server"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except Exception as e:
                last_error = f"Unexpected error: {e}"
                self.logger.error(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            if attempt < max_retries:
                self.logger.info(
                    f"Retrying in {retry_delay}s..."
                )
                time.sleep(retry_delay)

        self.logger.error(
            f"All {max_retries} attempts failed. Last error: "
            f"{last_error}"
        )
        return {"error": last_error or "Unknown error"}

    def execute_command(
        self, prompt: str
    ) -> Dict[str, Any]:
        """Send a command to the execute endpoint."""
        url = self.config.execute_endpoint
        payload = {"prompt": prompt}
        return self.post_json(url, payload)

    def describe_viewport(
        self, viewport_data: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Send viewport data for description."""
        url = self.config.describe_endpoint
        return self.post_json(url, viewport_data)


# Global client instance
_client: Optional[APIClient] = None


def get_client() -> APIClient:
    """Get or create the global API client."""
    global _client
    if _client is None:
        _client = APIClient()
    return _client
