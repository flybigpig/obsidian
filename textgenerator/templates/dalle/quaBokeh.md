---
promptId: 'quaBokeh'
name: '🖼️ Generate a Bokeh photo'
description: 'Enforce a large amount of background blur with clear outer bands, this can be used as a replacement for the “mm lens” prompts. Also could cause the subject to be closer to the camera.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,quality'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Bokeh
{{/run}}
```
***
***
{{get "r"}}