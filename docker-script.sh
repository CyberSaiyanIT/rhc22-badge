
mkdir -p /output/js

uglifyjs /public/js/client.js -o /output/js/client.js -c -m
uglifyjs /public/js/helpers.js -o /output/js/helpers.js -c -m
uglifyjs /public/js/index.js -o /output/js/index.js -c -m
uglifyjs /public/js/paginator.js -o /output/js/paginator.js -c -m
uglifyjs /public/js/tetris.js -o /output/js/tetris.js -c -m

node-sass /public/style.scss ./output/style.css --style compressed

node-minify --compressor html-minifier -i /public/index.html -o /output/index.html
node-minify --compressor html-minifier -i /public/tetris.html -o /output/tetris.html

sed -i 's:/backend/:/api/v1/:g' /output/js/client.js
cp /public/js/toastify.js /output/js
cp /public/js/webaudio-tinysynth.js /output/js
cp -r /public/css /output
cp -r /public/img /output
cp -r /public/tetris.mid /output