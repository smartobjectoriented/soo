.. _transcoder:

Transcoder logical block
------------------------

The transcoder is able to manage sending and receiving of ``block`` composed of several packets (payloads).

Currently, a block matches with a ME buffer entirely (a block = a ME).

The transcoder can manage several blocks associated to a ``sl_desc`` (SOOlink descriptor) up to ten blocks.

If the requester is not able to process blocks quickly enough, subsequent blocks will be discarded.


