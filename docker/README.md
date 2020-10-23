This folder contains a `Dockerfile` made to ease the compilation of the whole SOO framework.


## Build the image

To build the Docker image, simple execute the following command:

```bash
cd soo
docker build -t soo/build-env docker/
```

## Run the container

To run the previously built Docker image into a container, execute the following command:

```bash
docker run --rm -it -v $PWD:/home/reds/soo --name soo-buildenv soo/build-env
```

This will launch a SSH server running in foreground. If you wish to execute the container in background, add the `-d`
argument to the previous command.


## Connecting to the container

First, you should retrieve the IP address of the running container. To do so, `grep` the `IPAddress` field of the
`docker inspect soo-buildenv` command. It should return an address with the following format: `172.17.0.X`.

Then, `ssh` into the Docker container: `ssh reds@172.17.0.X`. The password is `reds`. You can then navigate to
`/home/reds/soo` and build the project!


## Killing the container

When you are done with compiling, you can simply kill the container by executing the command `docker kill soo-buildenv`.