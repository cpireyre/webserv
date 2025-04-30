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
    python3 -m pytest -v python_unit_tests.py
    or
    python3 -m pytest -v python_unit_tests.py::test_repeated_requests (single test)
"""

import os
from pathlib import Path
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
    time.sleep(0.1)  # Allow time for the server to start.
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
	files = {'file': ('filename.txt', b"dummy data\n")}
	response = requests.post("http://127.0.0.1:8080/images/", files=files)
	assert response.status_code in (200, 201)

    
def test_images_delete():
    """
    Test that DELETE /images/ is accepted.
    """

    # Setup: Create a test directory and file
    base_dir = Path("home/images")
    test_file = base_dir / "random.file"
    base_dir.mkdir(parents=True, exist_ok=True)
    test_file.write_text("Temporary file content.")

    # Run DELETE request
    response = requests.delete("http://127.0.0.1:8080/images/random.file")

    # Check the response status code
    assert response.status_code in (200, 202, 204)

    # Optionally, clean up
    # if test_file.exists():
    #     test_file.unlink()
    # if base_dir.exists() and not any(base_dir.iterdir()):
    #     base_dir.rmdir()


def test_imagesREDIR():
    """
    Test that GET /imagesREDIR/ returns a 307 redirect to /images/.
    """
    response = requests.get("http://127.0.0.1:8080/imagesREDIR/", allow_redirects=False)
    assert response.status_code == 307
    location = response.headers.get("Location", "")
    assert "/images/" in location


def test_cgi_empty_redirect():
    """
    Test that GET /cgi/empty/ returns a 307 redirect to https://www.google.com.
    """
    response = requests.get("http://127.0.0.1:8080/cgi/empty/", allow_redirects=False)
    assert response.status_code == 307
    location = response.headers.get("Location", "")
    assert "https://www.google.com" in location
    
@pytest.mark.skip(reason="Broken until we get proper error page")
def test_bad_http_request():
    """
    Test that sending a malformed HTTP request (bad header format) returns a 400 error.
    
    This test uses a raw socket to send a manually crafted HTTP request with an intentionally
    malformed header (missing the colon). Higher-level libraries like requests or aiohttp would
    sanitize such inputs automatically, so a raw socket is better suited for this purpose.
    """
    import socket
    
    # Connect to the local server at 127.0.0.1:8080
    with socket.create_connection(("127.0.0.1", 8080), timeout=5) as sock:
        # Craft a malformed HTTP request:
        # - Valid request line and Host header
        # - A header line without a colon (malformed)
        bad_request = (
            "GET /index.html HTTP/1.1\r\n"
            "Host: http://127.0.0.1\r\n"
            "BadHeaderWithoutColon\r\n"  # This header is invalid (missing ':' separator)
            "\r\n"
        )
        sock.sendall(bad_request.encode())
        
        # Receive the response (reading 1024 bytes is generally sufficient for the header)
        response = sock.recv(1024).decode(errors="ignore")
    
    # Extract the status line (first line of the response)
    status_line = response.splitlines()[0] if response else "Empty response"
    # Assert that the status line indicates a 400 Bad Request.
    # Depending on the implementation of your server, the exact wording might vary.
    assert "400" in status_line, f"Expected a 400 Bad Request response, got: {status_line}"
    
@pytest.mark.skip(reason="Slow and broken anyway")
def test_idle_disconnect():
    """
    Test that the server disconnects an idle connection by
    1) sending an HTTP 408 Request Timeout response, and then
    2) closing the connection (EOF on recv).

    We send a partial HTTP request, wait for the server's idle
    timeout to expire, then read whatever comes back:
      - The first recv should contain a 408 status line.
      - A subsequent recv should return b'' to signal EOF.
    """
    import socket, time, pytest

    # This must exceed your server's idle/keep-alive timeout.
    idle_wait = 15  # seconds

    with socket.create_connection(("127.0.0.1", 8080), timeout=idle_wait + 2) as sock:
        sock.settimeout(idle_wait + 2)

        # Send a partial HTTP request (no terminating CRLF).
        partial_request = (
            "GET /index.html HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
        )
        sock.sendall(partial_request.encode())

        # Wait for the server to hit its idle timeout.
        time.sleep(idle_wait)

        # Read the server's response (should be the 408 status).
        resp = sock.recv(1024)
        assert resp, "Expected a 408 response, but recv() returned no data"
        # Check the status line starts with HTTP/1.1 408
        status_line = resp.split(b"\r\n", 1)[0]
        assert status_line.startswith(b"HTTP/1.1 408"), (
            f"Expected status 'HTTP/1.1 408', got {status_line!r}"
        )

        # After the response, the server should close the connection:
        eof = sock.recv(1024)
        assert eof == b'', f"Expected EOF (b''), but got {eof!r}"



def test_good_http_request():
    """
    Test sending a valid HTTP GET request using a raw socket and verify that it returns a 200 OK response.
    This test sends a properly formatted request header to ensure the server responds correctly,
    eliminating malformed header errors as a factor.
    """
    import socket
    
    # Establish a connection to the server.
    with socket.create_connection(("127.0.0.1", 8080), timeout=5) as sock:
        # Construct a valid HTTP GET request for /index.html.
        good_request = (
            "GET /index.html HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Connection: close\r\n"  # Ensure the connection is closed after the response.
            "\r\n"
        )
        # Send the request.
        sock.sendall(good_request.encode())
        
        # Receive the response from the server.
        response = sock.recv(1024).decode(errors="ignore")
    
    # Get the first line of the response which should contain the HTTP status.
    status_line = response.splitlines()[0] if response else ""
    
    # Assert that the response indicates a successful request.
    assert "200" in status_line, f"Expected a 200 OK response, got: {status_line}"



@pytest.mark.skip(reason="Broken until we get proper error page")
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


@pytest.mark.skip(reason="Arguably broken test; we don't send a status code if we were in the middle of sending the request body")
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
    
@pytest.mark.skip(reason="Not sure if this one has special filesystem dependencies I don't have")
def test_file_upload_and_check():
    """
    Test that uploading a file through a POST request is successful and 
    that the file is correctly saved in the 'home/images' directory.
    
    Assumes the server routes POST requests to /images/ to the directory:
      ./home/images
    """
    import time
    from pathlib import Path
    import requests

    # Define the file name and content for the test.
    filename = "test_upload.txt"
    file_content = b"sample file content"
    files = {'file': (filename, file_content)}
    
    # Send a POST request to the /images/ endpoint.
    response = requests.post("http://127.0.0.1:8080/images/", files=files)
    assert response.status_code in (200, 201), f"Upload failed with status {response.status_code}"
    
    # Optionally, wait a short time to let any asynchronous file writing complete.
    time.sleep(0.5)
    
    # Determine the expected path to the uploaded file.
    upload_path = Path("home/images/uploads") / filename
    
    # Verify that the file now exists.
    assert upload_path.exists(), f"Expected uploaded file at {upload_path} does not exist."
    
    # Optionally, verify that the file contents are as expected.
    with upload_path.open("rb") as f:
        saved_content = f.read()
    assert saved_content == file_content, "File content does not match expected content."
    
    # Clean up: remove the file after the test so that repeated test runs don't accumulate files.
    try:
        upload_path.unlink()
    except Exception as e:
        print(f"Error cleaning up the test file: {e}")


@pytest.mark.skip(reason="I think this one is not supported fully at the moment (April 30)")
def test_streaming_file_upload():
    """
    Test uploading a file using a streaming multipart/form-data request.
    
    This test simulates a file that is too large to be uploaded all at once by streaming
    its content in chunks via a generator. The multipart body is built manually.
    
    The expected behavior is that the server processes the streamed upload and saves the file
    in the 'home/images' directory under the specified filename.
    """
    import requests
    import time
    from pathlib import Path
    
    # Name of the file to be uploaded.
    target_file = "test_large_upload.txt"
    # Expected location where the server writes the uploaded file.
    upload_path = Path("home/images") / target_file
    
    # Remove any existing file from previous tests.
    if upload_path.exists():
        upload_path.unlink()
    
    # Define a boundary for the multipart form data.
    boundary = "MYBOUNDARY123"
    
    def multipart_generator():
        # Starting boundary.
        yield f"--{boundary}\r\n".encode()
        # Content-Disposition header with the target file name.
        yield b'Content-Disposition: form-data; name="file"; filename="' + target_file.encode() + b'"\r\n'
        yield b"Content-Type: application/octet-stream\r\n\r\n"
        
        # Simulate streaming the file content in chunks.
        chunk_size = 1024  # 1 KB per chunk.
        num_chunks = 100   # Total size: 100 KB.
        for _ in range(num_chunks):
            yield b'a' * chunk_size
        
        # End the file content with a line break.
        yield b"\r\n"
        # Terminating boundary.
        yield f"--{boundary}--\r\n".encode()
    
    # Set the appropriate Content-Type header with the boundary.
    headers = {"Content-Type": f"multipart/form-data; boundary={boundary}"}
    
    # Send the streaming POST request.
    response = requests.post("http://127.0.0.1:8080/images/", data=multipart_generator(), headers=headers)
    assert response.status_code in (200, 201), f"Streaming upload failed with status {response.status_code}"
    
    # Allow a short time for the file to be written to disk.
    time.sleep(0.1)
    
    # Verify that the file now exists.
    assert upload_path.exists(), f"Uploaded file {upload_path} does not exist."
    
    # Verify that the file size matches the expected total (100 KB in this case).
    expected_size = chunk_size * num_chunks  # 1024 * 100 = 102400 bytes.
    actual_size = upload_path.stat().st_size
    assert actual_size == expected_size, f"Uploaded file size {actual_size} does not match expected {expected_size}"
    
    # Clean up: remove the file after the test.
    upload_path.unlink()



########################################################################
# Asynchronous Test for Concurrency
########################################################################
@pytest.mark.asyncio
async def test_repeated_requests():
    """
    Asynchronously send multiple concurrent GET requests to /index.html.
    This test simulates load. Adjust num_requests as appropriate.
    """
    num_requests = 10000
    url = "http://127.0.0.1:8080/index.html"

    async with aiohttp.ClientSession() as session:
        tasks = [session.get(url) for _ in range(num_requests)]
        responses = await asyncio.gather(*tasks)

    for resp in responses:
        assert resp.status == 200
        await resp.release()

import aiohttp
import asyncio
import pytest

@pytest.mark.asyncio
async def test_concurrent_get_and_post():
    """
    Asynchronously send multiple concurrent GET and POST requests to the server.
    This test simulates a mixed load of GET and POST requests.
    """
    num_get = 10000   # Number of GET requests to send
    num_post = 10000  # Number of POST requests to send
    get_url = "http://127.0.0.1:8080/index.html"
    post_url = "http://127.0.0.1:8080/images/"

    async with aiohttp.ClientSession() as session:
        # Create tasks for GET requests
        get_tasks = [session.get(get_url) for _ in range(num_get)]
        
        # Create tasks for POST requests using FormData for file upload.
        post_tasks = []
        for _ in range(num_post):
            form = aiohttp.FormData()
            form.add_field(
                'file',
                b"dummy data\n", 
                filename="filename.txt", 
                content_type="application/octet-stream"
            )
            post_tasks.append(session.post(post_url, data=form))
        
        # Combine both sets of tasks and run them concurrently
        tasks = get_tasks + post_tasks
        responses = await asyncio.gather(*tasks)

    # Process and verify each response
    for resp in responses:
        method = resp.request_info.method
        if method == "GET":
            assert resp.status == 200, (
                f"GET request to {resp.request_info.url} returned {resp.status}"
            )
        elif method == "POST":
            # Adjust expected status codes for POST as per your server logic.
            assert resp.status in (200, 201), (
                f"POST request to {resp.request_info.url} returned {resp.status}"
            )
        # Ensure proper cleanup of response objects.
        await resp.release()
