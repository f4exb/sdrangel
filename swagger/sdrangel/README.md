# Swagger project for SDRangel

<h1>What is Swagger?</h1>

Swagger is an open source software framework backed by a large ecosystem of tools that helps developers design, build, document, and consume RESTful Web services. [Link to Swagger website](https://swagger.io/)

Swagger is used to design the SDRangel web API. You will find the Swagger framework for SDRangel here. 

<h1>Install Swagger</h1>

Uses `apt-get` package manager (Ubuntu, Debian). Use your distribution package manager for other distributions

```shell
sudo apt-get install npm
sudo npm install -g swagger
```

install Node.js 6.x on Ubuntu 16.04 :

```shell
curl -sL https://deb.nodesource.com/setup_6.x | sudo -E bash -
sudo apt-get install -y nodejs
```
Install http-server a lightweight web server to serve included files

```shell
sudo npm install -g http-server
```

<h1>Work with Swagger</h1>

<h2>Edit files with Swagger</h2>

All commands are relative to this directory (where the README.md is)

Firstly start a node server to serve files

```shell
http-server --cors .
```

To start on a different port than 8080 that may be busy use the `-p` option:

```shell
http-server -p 8081 --cors .
```

Then in the directory where this README.md is start the swagger editor

```shell
swagger project edit
```

<h2>Mocking</h2>

Write controllers for mocking in `api/mocks/`

Run the server in mock mode:

```shell
swagger project start -m
```

<h2>Generating code</h2>

<h3>Installation</h3>

The released code generator is presently creating memory leaks for the Qt5/cpp generated code. A fixed version is available [here](https://github.com/etherealjoy/swagger-codegen/tree/qt5cpp_rework_antis81_patch-1).

So you will have to clone this repository and checkout the `qt5cpp_rework_antis81_patch-1` branch. Then follow the build instructions which are very simple when using maven:

```shell
sudo apt-get install maven # do this once to install maven
cd swagger-codegen
export JAVA_HOME=/opt/install/jdk1.8.0_172 # Example JDK change to your own
mvn clean package # let it compile...
mkdir -p /opt/install/swagger/swagger-codegen
cp modules/swagger-codegen-cli/target/swagger-codegen-cli.jar /opt/install/swagger
```

Then in the `/opt/install/swagger/` directory write a little `swagger-codegen` shell script to facilitate the invocation. For example:

```shell
#!/bin/sh
/opt/install/jdk1.8.0_74/bin/java -jar /opt/install/swagger/swagger-codegen-cli.jar ${*}
```
Give it execute permissions:

```shell
chmod +x /opt/install/swagger/swagger-codegen
```

Then invoke the generator with `/opt/install/swagger/swagger-codegen <commands>`

<h3>Code generation</h3>

Do not forget to start the http server for includes as described before for the editor.

Eventually to generate the code for Qt5 in the code/qt5 directory do:

```shell
/opt/install/swagger/swagger-codegen generate -i api/swagger/swagger.yaml -l qt5cpp -c qt5cpp-config.json -o code/qt5
```

The language option `-l` allows to generate code or documentation in a lot of languages (invoke the command without any parameter to get a list). The most commonly used are:

  - html2: documentation as a single HTML file pretty printed. Used to document the API in code/html2
  - python: Python client
  - angular2: Typescript for Angular2 client
  
The configuration option `-c` lets you specify a configuration file which is in JSON format and for which the key, value pairs are documented in the config-help of the specified codegen language. For example the following command will return the possible key values for Qt5 C++ generator:

```shell
/opt/install/swagger/swagger-codegen config-help -l qt5cpp
```

The following configuration files have been defined for generation in the SDRangel context:

  - `qt5cpp-config.json` for Qt5/C++
  - `html2-config.json` for HTML2
  - `python-config.json` for Python

<h3>Links</h3>

  - [Github repository](https://github.com/swagger-api/swagger-codegen)
  - [Server side code generation](https://github.com/swagger-api/swagger-codegen/wiki/Server-stub-generator-HOWTO) although not useful in this case

<h2>Show documentation with swagger-ui</h2>

<h3>Installation</h3>

Detailed instructions [here](https://swagger.io/docs/swagger-tools/#download-33)

  - Go to the [GitHub repository](https://github.com/swagger-api/swagger-ui) of the Swagger UI project
  - Clone the repository
  - Go to the cloned repository folder
  - Open the `dist/index.html` file with a browser
  - Say you started the node server on 127.0.0.1 port 8081 as in the example above (see: "Edit files with Swagger" paragraph)
  - In the "Explore" box at the top type: `http://127.0.0.1:8081/api/swagger/swagger.yaml`
  - Hit enter or click on the "Explore" button
