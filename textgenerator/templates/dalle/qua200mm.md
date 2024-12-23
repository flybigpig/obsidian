---
promptId: 'qua200mm'
name: '🖼️ Generate a 200mm lens photo'
description: 'Extremely zoomed in photo, tons of background blur, & will look like it was photographed from a far distance and then zoomed in a lot (good for photos of flying birds, small animals).'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,quality,lens'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, 200mm lens
{{/run}}
```
***
***
{{get "r"}}