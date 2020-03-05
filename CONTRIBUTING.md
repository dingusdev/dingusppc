General
=======

* Test your contributions to assure they do not break existing functionality.
* Commits to the repo should be done by portions; that is, by individual functionalities of the program.
* Don't use any disrespectful language.

Code
=======

* Make sure the code can compile for all target systems - Windows, Linux, and macOS/Mac OS X.
* All code must be compatible with at least C++11.
* Minimize the amount of redundant code.
* Avoid using absolute paths for the headers.
* Code should maintain vertical alignment for better readability, for example:

```C
one_hundred  =  100;
one_thousand = 1000;
two_thousand = 2000;
```

* CamelCase for class names, lowercase for variables, UPPERCASE for enumerations
* Avoid redundancy in namespaces (i.e. use ViaCuda::read() instead of ViaCuda::cuda_read())

Issues
=======

* When creating an issue ticket, note which version you are using.
* If possible, reference the git hashes for the relevant commits.
* Once an issue is closed, do not re-open it. Instead, reference it in a new issue ticket.
