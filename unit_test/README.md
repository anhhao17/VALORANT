**HOW TO RUN UNIT-TESTS**

*Activate python3 virtual environment (Optional)*

```shell
cd unit_test
python3 -m venv venv
source venv/bin/activate
```

*Install requirements (One time)*

```shell
cd unit_test
pip install -r requirements.txt
```

*Run any python script to start the test*

example:
```shell
python3 api_login.py
python3 api_system.py
python3 api_config.py
python3 api_users.py
python3 api_hardware.py
python3 api_streaming.py
```

*Run all tests*

```shell
cd unit_test
python3 run_all_tests.py
```

**Available Test Scripts:**

- `api_login.py` - Authentication and login tests
- `api_system.py` - System information and status tests
- `api_config.py` - Configuration management tests
- `api_users.py` - User management tests
- `api_hardware.py` - Hardware monitoring tests
- `api_streaming.py` - Streaming management tests

**Test Requirements:**

- Jetson BMCweb server must be running on http://localhost:8080
- Default admin credentials: admin/admin
- Python 3.6+ with requests library
