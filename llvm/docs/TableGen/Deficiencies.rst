=====================
TableGen Deficiencies
=====================

.. contents::
   :local:

Introduction
============

Despite being very generic, TableGen has some deficiencies that have been
pointed out numerous times.

TableGen is written in C++.  C++ is fast, but since TableGen essentially
converts text-format files to text-format files, TableGen requires a 
great deal of internal complexity to express relatively simple ideas.
Hacking TableGen takes more time than it ought.

As of this writing, for many important TableGen concepts, the documentation
is the source code itself.  This is a side effect of TableGen's 
complexity.

TableGen's declarative language has been called upon to do more and more
over the years, and it is not growing gracefully.  Many TableGen 
concepts were added as needed, not part of a grand design, and the 
inconsistent language treatment of similar ideas reflects this.  TableGen
also suppports per-platform behavior, increasing its complexity for each
supported platform.

The common theme is that, while TableGen allows you to build
Domain-Specific-Languages, the final languages that you create
lack the power of other DSLs, which in turn increase considerably the size
and complexity of TableGen files.

There are some in favour of extending the semantics even more, while making sure
back-ends adhere to strict rules. Others suggest we should move to more
powerful DSLs designed with specific purposes, or even re-use existing
DSLs.

While there are reasons to dislike TableGen language, there is a vast amount
of work currently invested in the language.  See for example the X86 backend.
The effort required in replacing or transitioning TableGen should not be
underestimated.
