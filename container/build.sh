# podman does not handle permissions properly and won't install sppack packages due to untar issues

export PIPX=yes

#podman build --userns-uid-map=0:1:1024 --userns-gid-map=0:1:1024 --format docker --rm --volume /home/biddisco/src/grox:/grox:ro,Z -f ./Dockerfile-grox-spack
podman build --format docker --rm --volume /home/biddisco/src/grox:/grox:ro,Z -f ./Dockerfile-grox-spack

