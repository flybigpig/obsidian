---
promptId: 'quaTiltShift'
name: '🖼️ Generate a Tilt-shift photo'
description: 'select a text and Tilt-shift photo about it will be generated using Dalle-2'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,quality'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Tilt-shift photography
{{/run}}
```
***
***
{{get "r"}}