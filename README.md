# PlatformIO CI/CD template

This repository template contains all the necessary files to:

1. Do PlatformIO development with VSCode and remote development containers.
2. GitHub actions for CI/CD, including attaching firmware binaries automatically to every pull request
   and on every GitHub release.
3. Automatic version number assignment at build time. Local builds get version `0.0.1`, pull request
   builds get version `0.0.{pull request number}` and release builds get the version from the GitHub
   release tag.

## Using this repository template

Using this repository template is easy: just hit the _Use this template_ button and make a new repo. That's it!
