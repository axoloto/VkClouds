#!/bin/bash

set -euo pipefail

printf "========================= START $DEV_BUILD_TYPE BUILD ============================ \n"

paths=" - root folder = $DEV_DIR\n"
paths+=" - build folder = $DEV_BUILD_DIR\n"
paths+=" - install folder = $DEV_INSTALL_DIR\n"
paths+=" - glslc folder = $GLSLC_DIR\n"
printf "%b" "$paths\n"

SHADER_DIR="$DEV_INSTALL_DIR/$DEV_BUILD_TYPE/shaders/"
TEXTURE_DIR="$DEV_INSTALL_DIR/$DEV_BUILD_TYPE/textures/"
MODEL_DIR="$DEV_INSTALL_DIR/$DEV_BUILD_TYPE/models/"

mkdir -p "$DEV_BUILD_DIR/$DEV_BUILD_TYPE" "$DEV_INSTALL_DIR/$DEV_BUILD_TYPE"

cd "$DEV_BUILD_DIR/$DEV_BUILD_TYPE"

printf "========================= START CMAKE ============================ \n"

cmake "$DEV_DIR" \
      -DCMAKE_INSTALL_PREFIX="$DEV_INSTALL_DIR/$DEV_BUILD_TYPE" \
      -DCMAKE_BUILD_TYPE=$DEV_BUILD_TYPE \
      -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="$DEV_DIR/cmake/conan_provider.cmake" \
      -DSHADER_DIR=$SHADER_DIR \
      -DTEXTURE_DIR=$TEXTURE_DIR \
      -DMODEL_DIR=$MODEL_DIR

cmake --build "$DEV_BUILD_DIR/$DEV_BUILD_TYPE" --config "$DEV_BUILD_TYPE" --target "install"

printf "========================== END CMAKE ============================= \n"

printf "========================== START GLSLC ============================= \n"

mkdir -p $SHADER_DIR

for f in "$DEV_DIR/src/shaders/"*.vert; do
    filename=$(basename "$f" .vert)
    echo $filename
    "$GLSLC_DIR/glslc" "$f" -o "$SHADER_DIR/${filename}Vert.spv"
done

for f in "$DEV_DIR/src/shaders/"*.frag; do
    filename=$(basename "$f" .frag)
    echo $filename
    "$GLSLC_DIR/glslc" "$f" -o "$SHADER_DIR/${filename}Frag.spv"
done

for f in "$DEV_DIR/src/shaders/"*.comp; do
    filename=$(basename "$f" .comp)
    echo $filename
    "$GLSLC_DIR/glslc" "$f" -o "$SHADER_DIR/$filename.spv"
done

printf "========================== END GLSLC ============================= \n"

printf "========================== START TEXTURE HANDLING ============================= \n"

mkdir -p $TEXTURE_DIR

cp "$DEV_DIR/src/textures/viking_room.png" "$TEXTURE_DIR/viking_room.png"

printf "========================== END TEXTURE HANDLING ============================= \n"

printf "========================== START MODEL HANDLING ============================= \n"

mkdir -p $MODEL_DIR

#cp "$DEV_DIR/src/models/viking_room.obj" "$MODEL_DIR/viking_room.obj"

printf "========================== END MODEL HANDLING ============================= \n"

cd "$DEV_DIR"

printf "========================== END $DEV_BUILD_TYPE BUILD ============================ \n"
