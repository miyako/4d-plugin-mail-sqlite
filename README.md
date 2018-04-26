# 4d-plugin-mail-sqlite
Get meta information of emails managed by Apple Mail using SQLite3

***experimental***

### Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|||

### Version

<img src="https://cloud.githubusercontent.com/assets/1725068/18940649/21945000-8645-11e6-86ed-4a0f800e5a73.png" width="32" height="32" /> <img src="https://cloud.githubusercontent.com/assets/1725068/18940648/2192ddba-8645-11e6-864d-6d5692d55717.png" width="32" height="32" />

### Releases

[1.0](https://github.com/miyako/4d-plugin-mail-sqlite/releases/tag/1.0)

## Syntax

```
properties:=Mail Get message properties (messages)
```

Parameter|Type|Description
------------|------------|----
messages|ARRAY LONGINT|message ``id`` as seen in applescript
properties|TEXT|``JSON`` array of objects

### Discussion

It is well known that Apple Mail uses SQLite3 internally.

```bash
sqlite3 ~/Library/Mail/V5/MailData/Envelope\ Index
```

**Note**: The exact path depends on the macOS version.

There are 12 tables.

```bash
.tables
addresses                ews_folders              properties             
attachments              labels                   recipients             
duplicates_unread_count  mailboxes                subjects               
events                   messages                 threads   
```

This plugins attempts to fetch some meta information from this database by calling SQLite3 directly.

### Examples

```
ARRAY LONGINT($values;3)

$values{1}:=1238253
$values{2}:=1238259
$values{3}:=1238271

$json:=Mail Get message properties ($values)

SET TEXT TO PASTEBOARD($json)

C_OBJECT($o)
$o:=JSON Parse($json)
```

### The returned object

``UUID``: ``string`` the identifier used as folder name inside each ``mbox``  
``messages``: ``array`` of messages

### The message object

``attachments``: ``array`` of attachments  
``color``: ``number``  
``date_created``: ``number``  
``date_received``: ``number``  
``date_sent``: ``number``  
``flagged``: ``number``  
``flags``: ``number``  
``ID``: ``number``  
``mailbox``: ``string`` a URL style identifier of the local mailbox  
``recipients``: ``array`` of recipients  
``sender``: ``object``  
``sender.ID``: ``number``  
``sender.address``: ``string``  
``sender.name``: ``string`` display name  
``size``: ``number``  
``snippet``: ``string``  
``subject``: ``string``  
``type``: ``number``  

### The attachment object

``ID``: ``number``  
``name``: ``string`` file name  

### The recipient object

``ID``: ``number``  
``address``: ``string``  
``name``: ``string`` display name  

