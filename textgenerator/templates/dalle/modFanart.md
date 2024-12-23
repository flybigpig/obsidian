---
promptId: 'modFanart'
name: '🖼️ Generate a Fanart photo'
description: 'This gives the generation a cute young amateur graphic design feel, adding hearts to the image and so on.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2, modifier'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Fanart
{{/run}}
```
***
***
{{get "r"}}