"""
True async API client using threading to avoid blocking the editor.
"""
import json
import threading
import time
from queue import Queue
from typing import Any, Callable, Dict, Optional

try:
    import requests  # type: ignore
    HAS_REQUESTS = True
except ImportError:
    requests = None  # type: ignore  # Optional dependency
    HAS_REQUESTS = False

from ..core.config import get_config
from ..core.utils import Logger


class AsyncAPIClient:
    """
    Non-blocking API client using background threads.
    Prevents editor freezing during network requests.
    """

    def __init__(self):
        self.config = get_config()
        self.logger = Logger("AsyncAPIClient", self.config.verbose)
        self._response_queue: Queue = Queue()

    def post_json_async(
        self,
        url: str,
        payload: Dict[str, Any],
        callback: Callable[[Dict[str, Any]], None],
        timeout: Optional[float] = None,
    ) -> None:
        """
        Send POST request asynchronously.
        Callback will be invoked with result when complete.
        """
        if requests is None:
            error_result = {
                "error": "requests library not installed"
            }
            callback(error_result)
            return

        if timeout is None:
            timeout = self.config.get("timeout", 25)

        # Start background thread
        thread = threading.Thread(
            target=self._execute_request,
            args=(url, payload, callback, timeout),
            daemon=True,
        )
        thread.start()
        self.logger.debug(f"Started async request to {url}")

    def _execute_request(
        self,
        url: str,
        payload: Dict[str, Any],
        callback: Callable[[Dict[str, Any]], None],
        timeout: float,
    ) -> None:
        """Execute request in background thread."""
        if not HAS_REQUESTS or requests is None:
            callback({"error": "requests library not available"})
            return

        max_retries = self.config.get("max_retries", 3)
        retry_delay = self.config.get("retry_delay", 2.5)
        last_error = None

        for attempt in range(1, max_retries + 1):
            try:
                self.logger.debug(
                    f"POST {url} (attempt {attempt}/{max_retries})"
                )

                # requests is available here (checked above)
                response = requests.post(  # type: ignore[union-attr]
                    url, json=payload, timeout=timeout
                )
                response.raise_for_status()

                result = response.json()
                self.logger.debug(
                    f"Success: {len(str(result))} bytes"
                )

                # Invoke callback with result
                callback(result)
                return

            except requests.Timeout:  # type: ignore[union-attr]
                last_error = f"Request timed out after {timeout}s"
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except requests.HTTPError as e:  # type: ignore[union-attr]
                status = e.response.status_code
                last_error = (
                    f"HTTP {status}: {e.response.text[:200]}"
                )
                self.logger.warn(
                    f"{last_error} (attempt {attempt}/{max_retries})"
                )

            except requests.RequestException as e:  # type: ignore[union-attr]
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
                self.logger.info(f"Retrying in {retry_delay}s...")
                time.sleep(retry_delay)

        # All retries failed
        self.logger.error(
            f"All {max_retries} attempts failed. "
            f"Last error: {last_error}"
        )
        callback({"error": last_error or "Unknown error"})


# Global async client instance
_async_client: Optional[AsyncAPIClient] = None


def get_async_client() -> AsyncAPIClient:
    """Get or create the global async API client."""
    global _async_client
    if _async_client is None:
        _async_client = AsyncAPIClient()
    return _async_client
