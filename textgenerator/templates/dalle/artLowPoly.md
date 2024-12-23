---
promptId: 'artLowPoly'
name: '🖼️ Generate a low poly art photo'
description: 'select a text and photo with the style of low poly art will be generated using Dalle-2'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2, art'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, low poly art
{{/run}}
```
***
***
{{get "r"}}