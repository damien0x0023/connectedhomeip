#!/usr/bin/env python3

#
# Copyright (c) 2023 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import argparse
import os
import requests
import subprocess
import sys

def validate_token(token):
    url = "https://api.github.com/user"
    headers = {
        "Authorization": f"token {token}"
    }

    response = requests.get(url, headers=headers)
    
    if response.status_code == 200:
        print("[Validation result]:Token is valid.")
        return True
    elif response.status_code == 401:
        print("Error: Invalid or expired GitHub token.")
        return False
    else:
        print(f"Error: Failed to verify token. HTTP Status Code: {response.status_code}")
        return False

def main():

    try:
        zephyr_base = os.getenv("ZEPHYR_BASE")
        if not zephyr_base:
            zephyr_base = os.getenv("TELINK_ZEPHYR_BASE")
            os.environ['ZEPHYR_BASE'] = zephyr_base
        if not zephyr_base:
            raise RuntimeError(
                "No ZEPHYR_BASE environment variable found, please set ZEPHYR_BASE to a zephyr repository path.")

        parser = argparse.ArgumentParser(
            description='Script helping to update Telink Zephyr to specific revision.')
        parser.add_argument("hash", help="Update Telink Zephyr to specific revision.")
        parser.add_argument("remote", default="https://github.com/telink-semi/zephyr",
                            help="New remote URL for the Zephyr repository.")
        parser.add_argument("--token", help="GitHub token for accessing private repositories.")

        args = parser.parse_args()

        if args.token:
            token = args.token
            if not validate_token(token):
                sys.exit(1)
            repo_url = args.remote.replace("https://", f"https://{args.token}@")
        else:
            repo_url = args.remote

        print(f"Using repo URL: {repo_url}")

        remote_name='custom'

        command = ['git', '-C', zephyr_base, 'remote', 'add', remote_name, repo_url]
        subprocess.run(command, check=True)

        command = ['git', '-C', zephyr_base, 'fetch', remote_name]
        subprocess.run(command, check=True)

        command = ['git', '-C', zephyr_base, 'reset', args.hash, '--hard']
        subprocess.run(command, check=True)

        command = ['west', 'update', '-o=--depth=1', '-n', '-f', 'smart']
        subprocess.run(command, check=True)

        command = ['west', 'blobs', 'fetch', 'hal_telink']
        subprocess.run(command, check=True)

    except (RuntimeError, subprocess.CalledProcessError) as e:
        print(e)
        sys.exit(1)


if __name__ == '__main__':
    main()
