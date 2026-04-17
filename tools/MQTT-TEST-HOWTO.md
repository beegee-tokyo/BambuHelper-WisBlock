# MQTT Test Debug Guide

Use this guide when I ask you to collect a raw MQTT dump from your printer for debugging.

This is especially useful when a printer model exposes different field names than expected. For example, some H2D values do not match what I see on H2C, so I need a real dump from the affected printer to map the fields correctly.

## What this script does

`mqtt_test.py` connects to your Bambu printer over MQTT, requests a full `pushall` status payload, and saves the result to JSON files.

The script can create:

- `pushall_dump.json` - full printer status dump
- `ams_dump.json` - AMS-only extract, if AMS data is present

The most useful file for me is usually:

- `pushall_dump.json`

## Read this first

`mqtt_test.py` supports 2 connection modes:

- `lan` - direct connection to the printer on your local network
- `cloud` - connection through Bambu Cloud

Use only one mode at a time.

If I did not tell you which one to use:

- use `lan` first if your printer supports it and you already use LAN mode
- use `cloud` if LAN mode is unavailable, Developer Mode is disabled, or LAN testing does not work

For H2S, H2C, and H2D:

- `lan` mode requires both LAN mode and Developer Mode enabled on the printer
- `cloud` mode does not require Developer Mode

## Step 1: Install Python

If Python is already installed, you can skip this step.

### Windows

1. Go to https://www.python.org/downloads/
2. Download the latest Python 3 installer.
3. Run the installer.
4. Make sure `Add Python to PATH` is checked.
5. Finish the installation.

To verify it worked, open Command Prompt and run:

```bash
python --version
```

If that does not work, try:

```bash
py --version
```

### macOS / Linux

Open Terminal and check:

```bash
python3 --version
```

If Python is missing, install Python 3 using your normal system package manager or from https://www.python.org/downloads/

## Step 2: Get the files

You need:

- `tools/mqtt_test.py`

Optional helper for cloud token login:

- `tools/get_token.py`

If you already downloaded the BambuHelper repository, just open the `tools` folder and use the existing files.

If not, download the files from the repository and save them somewhere easy to find.

## Step 3: Open a terminal in the `tools` folder

Open Command Prompt, PowerShell, or Terminal in the same folder where `mqtt_test.py` is located.

If you downloaded the full repository, go into the `tools` folder first.

## Step 4: Install the required Python package for `mqtt_test.py`

Run one of these commands:

### Windows

```bash
python -m pip install paho-mqtt
   ```

If `python` does not work, try:

```bash
py -m pip install paho-mqtt
```

### macOS / Linux

```bash
python3 -m pip install paho-mqtt
   ```

## Step 5: Choose one mode

Now decide which mode you want to test:

- `LAN mode`
- `Cloud mode`

Follow only one section below.

## LAN mode

Use this if your printer is reachable directly on your local network.

### Before you run it

Please make sure:

- your printer is powered on
- your computer and printer are on the same local network
- LAN mode is enabled on the printer
- if you have an H2S, H2C, or H2D, Developer Mode is also enabled

You will need these values from the printer:

- printer IP address
- LAN access code
- serial number

### Edit the config for LAN mode

Open `mqtt_test.py` in any text editor and update the config block near the top of the file so it looks like this:

   ```python
MODE         = "lan"

PRINTER_IP   = "YOUR_PRINTER_IP"
ACCESS_CODE  = "YOUR_ACCESS_CODE"

CLOUD_TOKEN  = "YOUR_CLOUD_TOKEN"
CLOUD_REGION = "us"

SERIAL       = "YOUR_SERIAL_NUMBER"
```

Replace these values:

- `PRINTER_IP` with your printer IP address
- `ACCESS_CODE` with your 8-character LAN access code
- `SERIAL` with your printer serial number

Important:

- `SERIAL` must be uppercase
- do not remove the `CLOUD_TOKEN` or `CLOUD_REGION` lines, just leave them unused in LAN mode

Where to find the values:

- IP address: printer Settings > Network
- Access code: printer Settings > LAN Only Mode
- Serial number: printer Settings > Device > Serial Number

### Run the script in LAN mode

### Windows

```bash
python mqtt_test.py
```

If needed:

```bash
py mqtt_test.py
```

### macOS / Linux

```bash
python3 mqtt_test.py
```

Let it run for about 30 seconds.

## Cloud mode

Use this if:

- I specifically asked for cloud testing
- LAN mode is not available on your printer
- LAN mode is enabled but still does not return data
- you do not want to enable Developer Mode on H2S, H2C, or H2D

For cloud mode you need:

- your printer serial number
- a valid Bambu Cloud token
- the correct cloud region

Important:

- for EU accounts, use `CLOUD_REGION = "us"`
- use `CLOUD_REGION = "cn"` only for China-region accounts
- `SERIAL` must be uppercase

### Get your cloud token

Choose one of the methods below.

### Option A: Get the token from your browser

This is usually the easiest method.

1. Open https://bambulab.com and sign in to your Bambu account.
2. Open your browser developer tools.
3. Find the cookies for `bambulab.com`.
4. Copy the value of the cookie named `token`.

Quick browser hints:

- Chrome / Edge: press `F12`, open `Application`, then `Cookies`, then `https://bambulab.com`
- Firefox: press `F12`, open `Storage`, then `Cookies`, then `https://bambulab.com`
- Safari: open Web Inspector, then `Storage`, then `Cookies`

Copy the full token value exactly as it appears.

### Option B: Use the helper script

If you prefer, you can use `tools/get_token.py`.

First install its required package:

### Windows

```bash
python -m pip install curl_cffi
```

If needed:

```bash
py -m pip install curl_cffi
```

### macOS / Linux

```bash
python3 -m pip install curl_cffi
```

Then run:

### Windows

```bash
python get_token.py
```

If needed:

```bash
py get_token.py
   ```

### macOS / Linux

```bash
python3 get_token.py
```

The script will ask for:

- your Bambu account email
- your password
- your 2FA code, if your account uses 2FA

At the end it will print your access token. Copy it exactly.

### Edit the config for Cloud mode

Open `mqtt_test.py` in any text editor and update the config block near the top of the file so it looks like this:

```python
MODE         = "cloud"

PRINTER_IP   = "YOUR_PRINTER_IP"
ACCESS_CODE  = "YOUR_ACCESS_CODE"

CLOUD_TOKEN  = "YOUR_CLOUD_TOKEN"
CLOUD_REGION = "us"

SERIAL       = "YOUR_SERIAL_NUMBER"
   ```

Replace these values:

- `CLOUD_TOKEN` with your real Bambu Cloud token
- `CLOUD_REGION` with `us` or `cn`
- `SERIAL` with your printer serial number

Important:

- for most users, including EU accounts, `CLOUD_REGION` should stay `us`
- only use `cn` if your Bambu account is on the China server
- do not remove the `PRINTER_IP` or `ACCESS_CODE` lines, just leave them unused in Cloud mode

### Run the script in Cloud mode

### Windows

```bash
   python mqtt_test.py
   ```

If needed:

```bash
py mqtt_test.py
```

### macOS / Linux

```bash
python3 mqtt_test.py
```

Let it run for about 30 seconds.

## Step 6: Check the generated files

If the connection works and the printer sends data, the script should create:

- `pushall_dump.json`
- `ams_dump.json`

These files are usually created in the same folder where you ran the script.

## Step 7: Send the results to me

Preferred option:

- send `pushall_dump.json` by email to `keralots@gmail.com`

Why this is preferred:

- it contains the full raw payload
- I can check chamber temperature fields, AMS fields, heater fields, and any printer-model-specific naming differences

Smaller alternative if you do not want to send the full dump:

- send `ams_dump.json`
- or send only the `ams` section from `pushall_dump.json`
- if possible, also include any fields related to `chamber`, `temp`, `heater`, or `heat`

You may redact private values like the serial number if you want, but please do not change the JSON field names or structure.

## What I need most

For unusual printer-specific bugs, the most helpful file is:

- `pushall_dump.json`

For AMS-only investigation, the next best option is:

- `ams_dump.json`

## Common problems

### `python` is not recognized

Try:

```bash
py mqtt_test.py
```

or:

```bash
python3 mqtt_test.py
```

### `ModuleNotFoundError: No module named 'paho'`

Install the package first:

```bash
python -m pip install paho-mqtt
```

or:

```bash
py -m pip install paho-mqtt
```

### `ModuleNotFoundError: No module named 'curl_cffi'`

This only matters if you are using `get_token.py`.

Install the package first:

```bash
python -m pip install curl_cffi
   ```

or:

```bash
py -m pip install curl_cffi
```

### The script connects but receives no data in LAN mode

Please double-check:

- printer IP address
- access code
- serial number is uppercase
- printer is on the same network
- LAN mode is enabled
- on H2S, H2C, and H2D: Developer Mode is enabled

### Authentication failed in LAN mode

The LAN access code is usually incorrect. Re-check it on the printer screen and run the script again.

### Authentication failed in Cloud mode

Usually this means:

- the cloud token is expired
- the token was copied incorrectly
- the wrong `CLOUD_REGION` was used

Try getting a fresh token and check the region again.

### Cloud mode says the token is invalid or userId cannot be extracted

Make sure you copied the full token exactly as provided by the browser or `get_token.py`.

Do not copy only part of it.

## Quick summary

1. Install Python 3.
2. Install `paho-mqtt`.
3. Choose `lan` or `cloud`.
4. Edit the config in `mqtt_test.py`.
5. Run the script for 30 seconds.
6. Send me `pushall_dump.json`, or at minimum the AMS-related extract.

Thanks for helping me debug printer-specific MQTT data. It makes it much easier to fix support for new models and edge cases.
