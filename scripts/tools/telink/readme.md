# Manufacturing Partition Generator Utility

## Install python dependencies
```
python3 -m pip install -r requirements.txt
```

## Generate a factory partition
```
python3 mfg_tool.py --count 4 -v 0xFFF2 -p 0x8001 \
--serial-num AABBCCDDEEFF11223344556677889900 \
--vendor-name "Telink Semiconductor" \
--product-name "not-specified" \
--mfg-date 2022-02-02 \
--hw-ver 1 \
--hw-ver-str "prerelase" \
--pai \
--key /path/to/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem \
--cert /path/to/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem \
-cd /path/to/connectedhomeip/credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der  \
--spake2-path /path/to/spake2p \
--chip-tool-path /path/to/chip-tool \
--chip-cert-path /path/to/chip-cert
```

## Generate 5 factory partitions [Optional argument : --count]
```
python3 mfg_tool.py --count 5 -v 0xFFF2 -p 0x8001 \
--serial-num AABBCCDDEEFF11223344556677889900 \
--vendor-name "Telink Semiconductor" \
--product-name "not-specified" \
--mfg-date 2022-02-02 \
--hw-ver 1 \
--hw-ver-str "prerelase" \
--pai \
--key /path/to/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem \
--cert /path/to/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem \
-cd /path/to/connectedhomeip/credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der  \
--spake2-path /path/to/spake2p \
--chip-tool-path /path/to/chip-tool \
--chip-cert-path /path/to/chip-cert
```

## Output files and directory structure
```
out
└── fff2_8001
    ├── aabbccddeeff11223344556677889900
    │   ├── factory_data.bin
    │   ├── factory_data.hex
    │   ├── internal
    │   │   ├── DAC_cert.der
    │   │   ├── DAC_cert.pem
    │   │   ├── DAC_key.pem
    │   │   ├── DAC_private_key.bin
    │   │   ├── DAC_public_key.bin
    │   │   └── pai_cert.der
    │   ├── onb_codes.csv
    │   ├── pin_disc.csv
    │   ├── qrcode.png
    |   └── summary.json
    ├── aabbccddeeff11223344556677889901
    │   ├── factory_data.bin
    │   ├── factory_data.hex
    │   ├── internal
    │   │   ├── DAC_cert.der
    │   │   ├── DAC_cert.pem
    │   │   ├── DAC_key.pem
    │   │   ├── DAC_private_key.bin
    │   │   ├── DAC_public_key.bin
    │   │   └── pai_cert.der
    │   ├── onb_codes.csv
    │   ├── pin_disc.csv
    │   ├── qrcode.png
    |   └── summary.json
    ├── aabbccddeeff11223344556677889902
    │   ├── factory_data.bin
    │   ├── factory_data.hex
    │   ├── internal
    │   │   ├── DAC_cert.der
    │   │   ├── DAC_cert.pem
    │   │   ├── DAC_key.pem
    │   │   ├── DAC_private_key.bin
    │   │   ├── DAC_public_key.bin
    │   │   └── pai_cert.der
    │   ├── onb_codes.csv
    │   ├── pin_disc.csv
    │   ├── qrcode.png
    |   └── summary.json
    └── aabbccddeeff11223344556677889903
        ├── factory_data.bin
        ├── factory_data.hex
        ├── internal
        │   ├── DAC_cert.der
        │   ├── DAC_cert.pem
        │   ├── DAC_key.pem
        │   ├── DAC_private_key.bin
        │   ├── DAC_public_key.bin
        │   └── pai_cert.der
        ├── onb_codes.csv
        ├── pin_disc.csv
        ├── qrcode.png
        └── summary.json
```