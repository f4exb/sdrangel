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

Then in start the swagger editor

```shell
swagger project edit
```

<h2>Mocking</h2>

Write controllers for mocking in `api/mocks/`

Run the server in mock mode:

```shell
swagger project start -m
```


