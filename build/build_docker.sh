#!/bin/bash
cp -r ../hook-api-examples docker/js #docker doesnt like symlinks?
docker build --tag richardah/xrpld-hooks-tech-preview:latest . && docker create rippled-hooks
rm -rf docker/js
docker push richardah/xrpld-hooks-tech-preview:latest
