---
promptId: 'ligAmbient'
name: '🖼️ Generate a Ambient Lighting photo'
description: 'select a text and ambient lighting photo about it will be generated using Dalle-2'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,lighting'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Ambient Lighting
{{/run}}
```
***
***
{{get "r"}}