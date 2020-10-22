This folder contains a `Dockerfile` made to ease the compilation of the whole SOO framework.


## Build the image

To build the Docker image, simple execute the following command:

```bash
docker build -t soo/build-env path/to/soo/docker
```

## Run the image

To run this Docker image, execute the following command:

```bash
docker run --rm -it -v /absolute/path/to/soo:/home/reds/soo soo/build-env
```

This will launch a SSH server. To launch it in background, add the `-d` argument to the previous command.


## Build the project

First, `ssh` into your docker container: `ssh reds@172.17.0.2`. The password is `reds`. You can then navigate to
`/home/reds/soo` and build the project!