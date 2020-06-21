<h1>Running Swagger code generation in Docker containers</h1>

The `server` folder contains files to build the http server image used to serve the files referenced in the `$ref` field of Swagger `.yaml` files.

The `codegen` folder contains files to build the code generator `swagger-codegen` used to generate the source and documentary files used to support SDRangel REST API.

The `compose` folder contains files to build the Docker compose stack comprising the `server` and `codegen` containers.

The SDRangel source tree is mounted in both containers as writable so the generated code can be written directly from the code generator or the Swagger files served by the http server.

<h2>Server</h2>

Use `build.sh` to build the image. It takes the following arguments:
  - `-t`: Image tag name `latest` is the default. The image is `sdrangel/swagger/server:<tag>`
  - `-f`: Specify a Dockerfile (default is Dockerfile in current directory i.e. '.')

The `run.sh` script is used to run the image on its own and is used for development only

<h2>Codegen</h2>

Use `build.sh` to build the image. It takes the following arguments:
  - `-b`: [Git repository](https://github.com/f4exb/swagger-codegen.git) branch name (default `sdrangel`)
  - `-c`: Arbitrary clone label. Clone again if different from the last label (default current timestamp)
  - `-t`: Docker image tag. Use git tag or commit hash (default `latest`)
  - `-f`: Specify a Dockerfile (default is Dockerfile in current directory i.e. '.')

<h2>Compose</h2>

Use `run.sh` to create or delete the Docker compose stack. It takes the following arguments:
  - `-D`: Use this option to bring down the compose stack (default is to bring up).
  - `-t`: Docker codegen image tag (default `latest`)
  - `-T`: Docker server image tag (default `latest`)
  - `-b`: SDRangel source code root path (default `/opt/build/sdrangel`)
  - `-c`: Compose stack name (default `sdrangelswg`)

The stack is composed of two containers sharing the `172.20.0.0/16` network internally.
  - `sdrangel_swgserver`: The http server that listens on port `8081` serving files in `/opt/build/sdrangel/swagger/sdrangel`
  - `sdrangel_swgcodegen`: The container with the Swagger code generator. The working directory is `/opt/build/sdrangel/swagger/sdrangel`.

Use `login.sh` to start a shell in the `sdrangel_swgcodegen` container. At the prompt run `generate.sh` to generate the code from the Swagger definition files.
