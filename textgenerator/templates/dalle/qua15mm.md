---
promptId: 'qua15mm'
name: '🖼️ Generate a 15mm wide-angle lens photo'
description: 'Very wide image with lots of information in the image.'
author: 'Prompt Engineering Guide'
tags: 'photo,dalle-2,quality,lens'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, 15mm wide-angle lens
{{/run}}
```
***
***
{{get "r"}}