"""PlatformIO pre-build script: load credentials from a .env file.

Reads KEY=VALUE pairs from the project-root .env (if present) and injects
them as string macros (-DKEY="value") for the firmware build.

Precedence: real environment variables win over .env entries, so CI can
override without editing the file.
"""

import os

Import("env")  # noqa: F821  (provided by SCons)

REQUIRED_KEYS = [
    "WIFI_SSID",
    "WIFI_PASSWORD",
    "TAPO_IP",
    "TAPO_EMAIL",
    "TAPO_PASSWORD",
]


def parse_dotenv(path):
    values = {}
    if not os.path.isfile(path):
        return values
    with open(path, encoding="utf-8") as fp:
        for raw in fp:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            if line.startswith("export "):
                line = line[len("export "):].lstrip()
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip()
            if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
                value = value[1:-1]
            if key:
                values[key] = value
    return values


dotenv_path = os.path.join(env.subst("$PROJECT_DIR"), ".env")
dotenv = parse_dotenv(dotenv_path)

missing = []
for key in REQUIRED_KEYS:
    value = os.environ.get(key) or dotenv.get(key)
    if not value:
        missing.append(key)
        continue
    env.Append(CPPDEFINES=[(key, env.StringifyMacro(value))])

if missing:
    print("[load_env] Missing values for: %s" % ", ".join(missing))
    print("[load_env] Set them in %s (see .env.example) or as environment variables."
          % dotenv_path)
elif dotenv:
    print("[load_env] Loaded credentials from %s" % dotenv_path)
