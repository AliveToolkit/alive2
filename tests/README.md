Alive2 unit tests
=================

three test file formats are supported:

- if a unit test has the suffix ".srctgt.ll" then this file will be sent to
  alive-tv. it should stand on its own.

- if a unit test has the suffix ".src.ll" then ".tgt.ll" must also exist, and
  this pair of files will be sent to alive-tv

- otherwise, the test is assumed to be written in the Alive domain
  specific language and it will be sent to alive
