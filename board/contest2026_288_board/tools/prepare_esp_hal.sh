#!/usr/bin/env bash
#
# Offline-first ESP HAL tree preparation for contest2026_288_board.
#
# Guarantees:
#   1. A chip/esp-hal-3rdparty tree at the pinned commit b90b1837cb5 exists.
#   2. The esp-hal-openvela-compat patch is applied.
#   3. The mbedtls submodule state (including the new mbedtls 4.x
#      tf-psa-crypto folder whose psa headers bootloader_support needs) is
#      complete — restored from the local backup tree when missing.
#
# It never contacts GitHub: the local pinned backup
#   <workspace>/vendor_esp32p4/chips/esp32p4/esp-hal-3rdparty
# is used when the in-board tree is incomplete.  The backup itself is never
# written to.

set -euo pipefail

ESP_HAL_COMMIT="b90b1837cb5ad24747deb4c895246037cc206ce5"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOARD_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ESP_HAL_DIR="${BOARD_DIR}/chip/esp-hal-3rdparty"
PATCH_FILE="${BOARD_DIR}/patches/esp-hal-openvela-compat.patch"
WORKSPACE="$(cd "${BOARD_DIR}/../../../.." && pwd)"
LOCAL_BACKUP="${WORKSPACE}/vendor_esp32p4/chips/esp32p4/esp-hal-3rdparty"
MBEDTLS_DIR="${ESP_HAL_DIR}/components/mbedtls/mbedtls"
MBEDTLS_BACKUP="${LOCAL_BACKUP}/components/mbedtls/mbedtls"

if [[ ! -d "${ESP_HAL_DIR}/.git" ]]; then
  if [[ -d "${LOCAL_BACKUP}/.git" ]]; then
    echo "esp-hal tree missing: restoring from offline backup (read-only)"
    cp -a "${LOCAL_BACKUP}" "${ESP_HAL_DIR}"
  else
    echo "error: no offline backup at ${LOCAL_BACKUP}" >&2
    exit 1
  fi
fi

# Bring mbedtls to a usable state.  The backup keeps a complete mbedtls at the
# pinned commit; use its tf-psa-crypto (and siblings) whenever parts of the
# submodule went missing.
if [[ ! -e "${MBEDTLS_DIR}/.git" ]]; then
  if [[ -d "${LOCAL_BACKUP}/components/mbedtls/mbedtls" ]]; then
    echo "mbedtls state missing (no .git): restoring from offline backup"
    rm -rf "${MBEDTLS_DIR}"
    cp -a "${LOCAL_BACKUP}/components/mbedtls/mbedtls" "${MBEDTLS_DIR}"
  else
    echo "error: mbedtls incomplete and no offline backup available" >&2
    exit 1
  fi
fi

# git-clean operations elsewhere can also drop untracked-but-needed folders
# such as tf-psa-crypto; restore just that folder from the backup if gone.
if [[ ! -d "${MBEDTLS_DIR}/tf-psa-crypto" ]]; then
  echo "tf-psa-crypto missing: copying from offline backup"
  cp -a "${LOCAL_BACKUP}/components/mbedtls/mbedtls/tf-psa-crypto" \
        "${MBEDTLS_DIR}/tf-psa-crypto"
fi

git -C "${ESP_HAL_DIR}" checkout --detach "${ESP_HAL_COMMIT}" 2>/dev/null || true

if git -C "${ESP_HAL_DIR}" apply --reverse --check "${PATCH_FILE}"; then
  echo "esp-hal compatibility patch is already applied"
elif git -C "${ESP_HAL_DIR}" apply --check "${PATCH_FILE}"; then
  git -C "${ESP_HAL_DIR}" apply "${PATCH_FILE}"
  echo "applied esp-hal compatibility patch"
else
  echo "error: esp-hal tree does not match the pinned commit or patch state" >&2
  exit 1
fi

echo "esp-hal-3rdparty ready at ${ESP_HAL_COMMIT}"
