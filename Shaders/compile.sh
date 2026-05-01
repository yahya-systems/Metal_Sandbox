#!/bin/bash
set -e
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
SRC_DIR="$SCRIPT_DIR/src"
IR_DIR="$SCRIPT_DIR/ir"
BUILD_DIR="$SCRIPT_DIR/build"

echo "--- gal Engine: Metal Library Build ---"

echo "[1/3] Clearing old artifacts..."
rm -rf "$IR_DIR" "$BUILD_DIR"
mkdir -p "$IR_DIR" "$BUILD_DIR"

echo "[2/3] Compiling source files to IR..."
for metal_file in "$SRC_DIR"/*.metal; do
  if [ -f "$metal_file" ]; then
    base_name=$(basename "$metal_file" .metal)
    echo "  -> Compiling: $base_name.metal"
    xcrun -sdk macosx metal -o "$IR_DIR/$base_name.ir" -c "$metal_file"
  fi
done

echo "[3/3] Linking IR files into separate .metallib files..."
for ir_file in "$IR_DIR"/*.ir; do
  if [ -f "$ir_file" ]; then
    base_name=$(basename "$ir_file" .ir)
    echo "  -> Linking: $base_name.metallib"
    xcrun -sdk macosx metallib "$ir_file" -o "$BUILD_DIR/$base_name.metallib"
  fi
done

echo "Done!"
echo "---------------------------------------"
