================
Remote Protocols
================

Firefox supports several remote protocols, which allow to inspect and control
the browser, usually for automation purposes:

* :ref:`marionette-header`
* :ref:`webdriver-bidi-header`

Common documentation
====================

The following documentation pages apply to all remote protocols

.. toctree::
  :maxdepth: 1

  Building.md
  Debugging.md
  Prefs.md
  Testing.md
  CodeStyle.md
  Security.md
  PuppeteerVendor.md

Protocols
=========

.. _marionette-header:

Marionette
----------

Marionette is used both by internal tools and testing solutions, but also by
geckodriver to implement the `WebDriver (HTTP) specification`_. The documentation
for Marionette can be found under `testing/marionette`_.

.. _WebDriver (HTTP) specification: https://w3c.github.io/webdriver/
.. _testing/marionette: /testing/marionette

.. _webdriver-bidi-header:

WebDriver BiDi
--------------

`The WebDriver BiDi specification <https://w3c.github.io/webdriver-bidi>`_
extends WebDriver HTTP to add bidirectional communication.

.. toctree::
  :maxdepth: 1

  webdriver-bidi/Extensions.md

Architecture
============

Message Handler
---------------

The documentation for the framework used to build WebDriver BiDi modules can be
found at `remote/messagehandler`_.

.. _remote/messagehandler: messagehandler/


Bugs
====

Bugs are tracked under the `Remote Protocol product`_.

.. _Remote Protocol product: https://bugzilla.mozilla.org/describecomponents.cgi?product=Remote%20Protocol


Communication
=============

See `Communication`_ on `our project wiki`_.

.. _Communication: https://wiki.mozilla.org/Remote#Communication
.. _our project wiki: https://wiki.mozilla.org/Remote
