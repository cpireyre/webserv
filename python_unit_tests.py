#!/usr/bin/env python3
"""
Comprehensive tests for the webserv project.

This file uses a function-scoped fixture to start a fresh server instance for every test
to avoid interference between tests.

The tests cover various aspects from the project specification and the given configuration file,
including:
  - Basic GET for the root ("/index.html")
  - Redirect behavior (e.g. /oldDir/ and /imagesREDIR/)
  - Testing endpoints with different allowed methods (GET, POST, DELETE)
  - Verifying a CGI redirect
  - Checking custom error responses for non-existent resources
  - Testing for oversized client request bodies

One asynchronous test simulates multiple concurrent GET requests.

Run tests with:
    python -m pytest -v python_unit_tests.py
"""

import subprocess
import time
import requests
import pytest
import asyncio
import aiohttp

########################################################################
# Fixture: Start a fresh server instance for each test (function-scoped)
########################################################################
@pytest.fixture(scope="function", autouse=True)
def start_server():
    print("\n=== Starting server ===")
    # Launch the server with the specified configuration file.
    proc = subprocess.Popen(["./webserv", "complete.conf"])
    time.sleep(1)  # Allow time for the server to start.
    yield proc
    print("=== Stopping server ===")
    proc.kill()


########################################################################
# Synchronous Tests
########################################################################

def test_get_root():
    """Test that GET /index.html returns 200."""
    response = requests.get("http://127.0.0.1:8080/index.html")
    assert response.status_code == 200


def test_oldDir_redirect():
    """
    Test that a GET request to /oldDir/ results in a 307 redirect to /newDir/.
    """
    response = requests.get("http://127.0.0.1:8080/oldDir/", allow_redirects=False)
    # Expect a redirection status code (307) and a Location header pointing to /newDir/
    assert response.status_code == 307
    location = response.headers.get("Location", "")
    assert "/newDir/" in location


def test_newDir_directory_listing():
    """
    Test that GET /newDir/ returns 200.
    The configuration enables directory listing in /newDir/ so we expect a listing.
    """
    response = requests.get("http://127.0.0.1:8080/newDir/")
    assert response.status_code == 200
    # Optionally, check for a marker (like an HTML tag) that indicates a directory listing.
    # For example, many servers include <title>Directory listing</title> in the response.
    content = response.text.lower()
    assert "directory" in content or "<html" in content


def test_images_get():
    """
    Test that GET /images/ (an endpoint allowing GET, POST, DELETE)
    returns 200 and contains (possibly) a directory listing.
    """
    response = requests.get("http://127.0.0.1:8080/images/")
    assert response.status_code == 200


def test_images_post():
    """
    Test that POST /images/ is accepted. We simulate a file upload with a small payload.
    (The expected behavior depends on your server implementation.)
    """
    payload = b"dummy data"
    response = requests.post("http://127.0.0.1:8080/images/", data=payload)
    # You might expect a 200 OK, 201 Created, or similar status code.
    # Adjust the expected status code as per your server’s behavior.
    assert response.status_code in (200, 201)


def test_images_delete():
    """
    Test that DELETE /images/ is accepted.
    """
    response = requests.delete("http://127.0.0.1:8080/images/")
    # Adjust the expected status code (200, 202, or 204 are common).
    assert response.status_code in (200, 202, 204)


def test_imagesREDIR():
    """
    Test that GET /imagesREDIR/ returns a 307 redirect to /images/.
    """
    response = requests.get("http://127.0.0.1:8080/imagesREDIR/", allow_redirects=False)
    assert response.status_code == 307
    location = response.headers.get("Location", "")
    assert "/images/" in location


def test_OdinEtchAndSketch():
    """
    Test that GET /Odin-Etch-And-Sketch-main/ (allowed methods are GET and POST)
    returns 200.
    """
    response = requests.get("http://127.0.0.1:8080/Odin-Etch-And-Sketch-main/")
    assert response.status_code == 200


def test_cgi_empty_redirect():
    """
    Test that GET /cgi/empty/ returns a 307 redirect to https://www.google.com.
    """
    response = requests.get("http://127.0.0.1:8080/cgi/empty/", allow_redirects=False)
    assert response.status_code == 307
    location = response.headers.get("Location", "")
    assert "https://www.google.com" in location


def test_not_found_error():
    """
    Test that a GET request for a non-existent resource returns a 404 error,
    which should trigger the custom error page.
    """
    response = requests.get("http://127.0.0.1:8080/nonexistent.html")
    # Depending on your implementation, the server should send a 404 and may serve the custom error page.
    assert response.status_code == 404
    # Optionally, check that the response content contains something indicative of the error page.
    content = response.text.lower()
    # For example, the custom error page may contain the error code or the phrase "not found".
    assert "404" in content or "not found" in content


def test_client_body_size_exceeded():
    """
    Test that sending a POST request with a large payload (exceeding max_client_body_size)
    results in an error (likely a 413 Payload Too Large error).
    """
    # Construct a payload larger than 5,000,000 bytes, e.g. 6,000,000 bytes.
    payload = b"x" * 6000000
    response = requests.post("http://127.0.0.1:8080/images/", data=payload)
    # Adjust the expected status code as needed; 413 is common for payload too large.
    assert response.status_code in (413, 400, 500), f"Unexpected status: {response.status_code}"


########################################################################
# Asynchronous Test for Concurrency
########################################################################
@pytest.mark.asyncio
async def test_repeated_requests():
    """
    Asynchronously send multiple concurrent GET requests to /index.html.
    This test simulates load. Adjust num_requests as appropriate.
    """
    num_requests = 100
    url = "http://127.0.0.1:8080/index.html"

    async with aiohttp.ClientSession() as session:
        tasks = [session.get(url) for _ in range(num_requests)]
        responses = await asyncio.gather(*tasks)

    for resp in responses:
        assert resp.status == 200
        await resp.release()
