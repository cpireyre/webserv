import subprocess, time, requests
import pytest

@pytest.fixture(scope="session", autouse=True)
def start_server():
    # Start the web server as a subprocess
    proc = subprocess.Popen(["./webserv", "complete.conf"])
    time.sleep(1)  # Give server time to start
    yield proc
    proc.kill()

def test_get_root():
    response = requests.get("http://127.0.0.1:8080/index.html")
    # Check that status code, content, etc. match your expectations.
    assert response.status_code == 200
    assert response.content == None
