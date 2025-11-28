# c3extractor

A very simple program to extract the assets from bundles used by the Construct 3 engine.

The program was made with Linux in mind, but I accept merge requests if anyone wants to port it to Windows or MacOS.

## Compilation

As of right now the program is designed to work only on linux. You can compile and install the program with the provided Makefile.

It uses gcc for compilation but you can edit the makefile to change it.

```bash
$ make install
```

## Usage

```bash
$ c3extractor <input file> <-o output directory>
```

The input file is usually an "assets.dat" file. The output directory is optional.
