import contextlib
import io
import json
import sys
import traceback


def main() -> int:
    code = sys.stdin.read()
    stdout_buffer = io.StringIO()
    stderr_buffer = io.StringIO()
    globals_dict = {"__name__": "__main__", "__builtins__": __builtins__}

    exit_code = 0
    success = True

    try:
        with contextlib.redirect_stdout(stdout_buffer), contextlib.redirect_stderr(stderr_buffer):
            exec(compile(code, "<llm-python-tool>", "exec"), globals_dict, globals_dict)
    except SystemExit as exc:
        success = False
        code_value = exc.code
        if isinstance(code_value, int):
            exit_code = code_value
        elif code_value is None:
            exit_code = 0
        else:
            exit_code = 1
            if code_value:
                print(code_value, file=stderr_buffer)
    except Exception:
        success = False
        exit_code = 1
        traceback.print_exc(file=stderr_buffer)

    payload = {
        "success": success and exit_code == 0,
        "exit_code": exit_code,
        "stdout": stdout_buffer.getvalue(),
        "stderr": stderr_buffer.getvalue(),
    }
    sys.stdout.write(json.dumps(payload, ensure_ascii=False))
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
