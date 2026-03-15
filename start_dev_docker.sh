# docker-compose -f docker-compose.dev.yml --verbose up --build
# docker-compose -f docker-compose.dev.yml up --build --no-cache
# docker compose -f docker-compose.dev.yml up --build --force-recreate
# docker-compose -f docker-compose.dev.yml build --no-cache
# docker run --rm -it sample4-web
# build docker image and run container
# docker build -t sample4-web -f Dockerfile.dev .
# docker run -p 3000:3000 -v .:/app -v /app/node_modules --rm -it sample4-web bin/dev
# docker-compose -f docker-compose.dev.yml --verbose up
# fixed error on docker, run this command on mac: bundle lock --add-platform aarch64-linux
docker-compose -f docker-compose.dev.yml up
