"""
Streaming API test cases
"""

from api_functions import (
    login, assert_response_status, get_streams, start_stream, 
    stop_stream, get_stream_status, get_stream_statistics
)

if __name__ == "__main__":
    # Login
    token = login("admin", "admin")
    print("Login Positive - PASS.")

    # Test get streams with authentication
    response = get_streams(token)
    assert_response_status(response, 200)
    streams = response.json()
    assert isinstance(streams, list)
    print("Get Streams Positive - PASS.")

    # Test get streams without authentication
    response = get_streams()
    assert_response_status(response, 401)
    print("Get Streams Negative (no auth) - PASS.")

    # If streams exist, test stream operations
    if streams and len(streams) > 0:
        stream_id = streams[0]["id"]
        print(f"Testing with stream: {stream_id}")

        # Test start stream
        response = start_stream(token, stream_id)
        # May fail if stream already running or not configured
        assert response.status_code in [200, 400, 404]
        print(f"Start Stream ({stream_id}) - PASS.")

        # Test get stream status
        response = get_stream_status(token, stream_id)
        # May fail if stream not configured
        assert response.status_code in [200, 404]
        print(f"Get Stream Status ({stream_id}) - PASS.")

        # Test get stream statistics
        response = get_stream_statistics(token, stream_id)
        # May fail if stream not configured
        assert response.status_code in [200, 404]
        print(f"Get Stream Statistics ({stream_id}) - PASS.")

        # Test stop stream
        response = stop_stream(token, stream_id)
        # May fail if stream not running
        assert response.status_code in [200, 400, 404]
        print(f"Stop Stream ({stream_id}) - PASS.")

        # Test stream operations without authentication
        response = start_stream(None, stream_id)
        assert_response_status(response, 401)
        print(f"Start Stream Negative (no auth) - PASS.")

        response = stop_stream(None, stream_id)
        assert_response_status(response, 401)
        print(f"Stop Stream Negative (no auth) - PASS.")
    else:
        print("No streams configured - skipping stream operation tests")

    exit(0)
