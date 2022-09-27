FROM node:latest

RUN npm install uglify-js node-sass @node-minify/cli@6.2.0 @node-minify/html-minifier@6.2.0 -g
ADD docker-script.sh /docker-script.sh

RUN chmod +x /docker-script.sh

ENTRYPOINT ["sh" , "/docker-script.sh" ]