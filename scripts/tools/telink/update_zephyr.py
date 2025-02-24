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
import yaml

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


def update_repo_url_with_token_in_west(repos, token, west_yml_path):
    try:
        with open(west_yml_path, 'r') as file:
            west_yml = yaml.safe_load(file)

        updated = False
        for project in west_yml.get('manifest', {}).get('projects', []):
            repo_name = project['name']
            if repo_name in repos:
                if not project['url'].startswith("https://"):
                    print(f"Error: URL for {repo_name} is not an HTTPS URL: {project['url']}")
                    sys.exit(1)

                new_url = project['url'].replace("https://", f"https://{token}@")
                print(f"Updating URL for {repo_name}: {project['url']} -> {new_url}")
                # subprocess.run(['west', 'config', f'projects.{repo_name}.url', new_url], check=True)
                project['url'] = new_url
                updated = True

        with open(west_yml_path, 'w') as file:
            yaml.safe_dump(west_yml, file)

    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to update URL for {repo_name}.")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

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

        token_valid = False
        if args.token:
            token = args.token
            token_valid = validate_token(token)
            if not token_valid:
                sys.exit(1)

            repo_url = args.remote.replace("https://", f"https://{args.token}@")
        else:
            repo_url = args.remote

        print(f"Using repo URL: {repo_url}")

        remote_name='custom'

        command = ['git', '-C', zephyr_base, 'remote', 'get-url', remote_name]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            command = ['git', '-C', zephyr_base, 'remote', 'add', remote_name, repo_url]
            subprocess.run(command, check=True)
        else:
            command = ['git', '-C', zephyr_base, 'remote', 'set-url', remote_name, repo_url]
            subprocess.run(command, check=True)

        command = ['git', '-C', zephyr_base, 'fetch', remote_name]
        subprocess.run(command, check=True)

        command = ['git', '-C', zephyr_base, 'reset', args.hash, '--hard']
        subprocess.run(command, check=True)

        repos_to_update = ['mcuboot', 'hal_telink']
        if token_valid:
            west_yml_path = os.path.join(zephyr_base, 'west.yml')
            if os.path.exists(west_yml_path):
                update_repo_url_with_token_in_west(repos_to_update, args.token, west_yml_path)
            else:
                print(f"Error: {west_yml_path} not found.")
                sys.exit(1)

        command = ['west', 'update', '-o=--depth=1', '-n', '-f', 'smart']
        subprocess.run(command, check=True)

        command = ['west', 'blobs', 'fetch', 'hal_telink']
        subprocess.run(command, check=True)

    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
