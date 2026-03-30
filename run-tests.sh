#!/bin/bash
set -e

# Per D-03: Single bash script at repo root, runnable from Git Bash
# Per D-02: Faithful replica of full CI (all 4 suites)
# Per D-05: Fresh compilation every run (no volume caching of build artifacts)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="proffie-tests"

echo "=== Building Docker test image ==="
docker build -f "$SCRIPT_DIR/Dockerfile.tests" -t "$IMAGE_NAME" "$SCRIPT_DIR"

echo ""
echo "=== Running test suites ==="

# Per D-01: All 4 directories from RunTests.yml, exact same order
# Per D-04: Script works from repo root
# Mount repo as read-write volume at /proffie (WORKDIR in Dockerfile)
# --rm removes container after run (clean up always)

MSYS_NO_PATHCONV=1 docker run --rm \
  -v "$SCRIPT_DIR:/proffie" \
  "$IMAGE_NAME" \
  bash -c '
    set -e
    PASS=0
    FAIL=0

    for suite in styles common blades sound; do
      echo ""
      echo "--- Testing $suite ---"
      if (cd "$suite" && make test); then
        echo "--- $suite: PASSED ---"
        PASS=$((PASS + 1))
      else
        echo "--- $suite: FAILED ---"
        FAIL=$((FAIL + 1))
      fi
    done

    echo ""
    echo "=== Results: $PASS passed, $FAIL failed ==="
    if [ "$FAIL" -gt 0 ]; then
      exit 1
    fi
  '

echo ""
echo "=== All test suites passed ==="
