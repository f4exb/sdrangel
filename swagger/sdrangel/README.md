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

Firstly start a node server to serve files in `api/swagger/include`

```shell
cd api/swagger/include
http-server --cors .
```

To start on a different port than 8080 use the `-p` option

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

The code generator is delivered in the form of a jar that you execute from the console using the java command. First make sure you have a JDK  or JRE version 1.7 or above. In our example it will be located in `/opt/install/jdk1.8.0_74/`

Download the jar from [this archive](https://oss.sonatype.org/content/repositories/releases/io/swagger/swagger-codegen-cli/). Choose the latest version ex: 2.2.3 which can be done with wget. Say you want to install the jar in `/opt/install/swagger`:

```shell
cd /opt/install/swagger
wget https://oss.sonatype.org/content/repositories/releases/io/swagger/swagger-codegen-cli/2.2.3/swagger-codegen-cli-2.2.3.jar
```

Then in the same directory write a little `swagger-codegen` shell script to facilitate the invocation. For example:

```shell
#!/bin/sh
/opt/install/jdk1.8.0_74/bin/java -jar /opt/install/swagger/swagger-codegen-cli-2.2.3.jar ${*}
```

Then invoke the generator with `/opt/install/swagger/swagger-codegen <commands>`

<h3>Code generation</h3>

Do not forget to start the http server for includes as described before for the editor.

Eventually to generate the code for Qt5 in the code/qt5 directory do:

```shell
/opt/install/swagger/swagger-codegen generate -i api/swagger/swagger.yaml -l qt5cpp -o code/qt5
```

The language option `-l` allows to generate code or documentation in a lot of languages (invoke the command without any parameter to get a list). The most commonly used are:

  - html2: documentation as a single HTML file pretty printed. Used to document the API in code/html2
  - python: Python client
  - angular2: Typescript for Angular2 client

<h3>Links</h3>

  - [Github repository](https://github.com/swagger-api/swagger-codegen)
  - [Server side code generation](https://github.com/swagger-api/swagger-codegen/wiki/Server-stub-generator-HOWTO) although not useful in this case

