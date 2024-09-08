#!/bin/bash
DIR="./public"
WWW_DIR="./data/www"
WORK_DIR=`mktemp -d`
PATH=./node_modules/.bin:$PATH

if [[ ! "$WORK_DIR" || ! -d "$WORK_DIR" ]]; then
  echo "Could not create temp dir"
  exit 1
fi

function cleanup {      
  rm -rf "$WORK_DIR"
  echo "Deleted temp working directory $WORK_DIR"
}

trap cleanup EXIT

cp -r "$DIR/css" "$DIR/img" "$DIR/tetris.mid" "$WORK_DIR"
mkdir -p "${WORK_DIR}/js"
cp "$DIR/js/toastify.js" "${WORK_DIR}/js"
cp "$DIR/js/webaudio-tinysynth.js" "${WORK_DIR}/js"
uglifyjs "$DIR/js/index.js" -o "$WORK_DIR/js/index.js"
uglifyjs "$DIR/js/client.js" -o "$WORK_DIR/js/client.js"
uglifyjs "$DIR/js/helpers.js" -o "$WORK_DIR/js/helpers.js"
uglifyjs "$DIR/js/paginator.js" -o "$WORK_DIR/js/paginator.js"
uglifyjs "$DIR/js/tetris.js" -o "$WORK_DIR/js/tetris.js"

node-sass "$DIR/style.scss" "$WORK_DIR/style.css" --style compressed
node-minify --compressor html-minifier -i "$DIR/index.html" -o "$WORK_DIR/index.html" 
node-minify --compressor html-minifier -i "$DIR/tetris.html" -o "$WORK_DIR/tetris.html"

if [[ $(uname -s) == 'Darwin' ]]; then
		sed -i.bak 's:/backend/:/api/v1/:g' "$WORK_DIR/js/client.js"
    rm "$WORK_DIR/js/client.js.bak"
else
		sed -i'' 's:/backend/:/api/v1/:g' "$WORK_DIR/js/client.js"
fi

cp -r $WORK_DIR/* "$DIR/../data/www/"
