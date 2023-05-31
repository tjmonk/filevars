# filevars
Variable templating service

## Overview

The filevars service is a variable templating service which maps variable templates to
variables.  When the variable is printed, the printing is handled by the variable
templating service which renders the variable template into an output while
replacing all variable references with their values.

## Variable references

Variables are referenced in a variable template within `${ }` tags.
Whenever a variable name is encountered, it is replaced with the variable value
via a call to the variable server.

For example, if `/sys/test/a` were an integer set to `5`, then any reference to `${/sys/test/a}`
in the template will be replaced with the value `5`.  The same holds true for all variable types.

## Filevars template file

An example template file is shown below:

```
------------------------------------------------------------------------------

This is a test of the variable server templating system.

It allows referencing of variables inside a template.

The variables will be replaced with their values rendered by the
variable server.

The value of "/sys/test/a" is "${/sys/test/a}" and the value of "/sys/test/b"
is "${/sys/test/b}".

It can handle variables of all types.

This one is a string: "${/sys/test/c}" and this one is a float: ${/sys/test/f}

------------------------------------------------------------------------------
```

As part of the build process, this template is copied to /usr/share/templates/test.tmpl

## Filevars configuration file

The filevars service is configured with a filevars JSON configuration file
which provides the mapping between the system variable name, and the
name of the template file to be associated with that variable.

An example configuration file is shown below:

```
{
    "config" : [
        { "var" : "/sys/test/info",
          "file" : "/usr/share/templates/test.tmpl" }
    ]
}
```

Note that multiple filevar mappings can be specified in a single configuration file,
and multiple instances of the filevars server can be invoked

## Prerequisites

The filevars service requires the following components:

- varserver : variable server ( https://github.com/tjmonk/varserver )
- tjson : JSON parser library ( https://github.com/tjmonk/libtjson )

## Building

```
$ ./build.sh
```

## Testing

### Create a variable to associate with a variable template

In this example the variable defaults to a 256 character string, but the variable associated
with the filevars can be of any type, even an integer as the variable output is overridden
by the filevars service, and the variable's value is never used.

```
$ mkvar /sys/test/info
```

### Get the variable and confirm it is blank

```
$ getvar /sys/test/info

```

### Start the filevars service

```
$ filevars -f test/filevars.json &
```

### Get the info variable and confirm it is rendered as per the template

```
$ getvar /sys/test/info
------------------------------------------------------------------------------

This is a test of the variable server templating system.

It allows referencing of variables inside a template.

The variables will be replaced with their values rendered by the
variable server.

The value of "/sys/test/a" is "15" and the value of "/sys/test/b"
is "-3".

It can handle variables of all types.

This one is a string: "Hello World" and this one is a float: 1.20

------------------------------------------------------------------------------
```
