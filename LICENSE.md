ElectroPaint for Haiku licensing
================================

Different parts of this project are covered by different licenses.

### src/ElectroPaintEngine.cpp, src/ElectroPaintEngine.h

These files are part of a decompilation of ElectroPaint, a work by David
Tristram, distributed with permission. ElectroPaint is a US trademark owned by
Tristram Visual, a sole proprietorship operating in California. Inquiries may
be directed to info@tristram.com and www.tristram.com.

Some further clarification from an email conversation with David Tristram:

"The implication regarding derived works is simply that any code that you may
produce that includes any part of ElectroPaint does not belong to you, so you
can't sell it, for example, or if you do, I'd have a legal interest in the
proceeds. If you are interested in modifying the program's behavior or
appearance, I think that's okay as long as you share the modified source with
me. All this includes decompiled source recovered from the binaries."

The engine is a rewrite of ep.c from ElectroPortis
(https://github.com/drvink/electroportis), Mark Laws' decompilation of the
IRIX binary, and is distributed under the same terms.

### All other files

Copyright 2026, the ElectroPaint for Haiku authors.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

The test harness in tests/reference fetches and compiles ElectroPortis' ep.c
(David Tristram's terms above) for comparison; it is not part of the package.
