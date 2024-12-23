---
promptId: 'artGraffiti'
name: '🖼️ Generate a graffiti photo'
description: 'select a text and photo with the style of graffiti will be generated using Dalle-2'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2, art'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, graffiti art
{{/run}}
```
***
***
{{get "r"}}