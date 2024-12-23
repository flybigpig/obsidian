---
promptId: 'ligFlare'
name: '🖼️ Generate a Lens Flare photo'
description: 'Adds a streak of light onto an image generation, creating the appearance of a bright light source being just outside of the frame.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,lighting'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Lens Flare
{{/run}}
```
***
***
{{get "r"}}