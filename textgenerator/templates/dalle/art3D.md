---
promptId: art3D
name: 🖼️ Generate a 3D art photo
description: select a text and photo with the style of 3D art will be generated using Dalle-2
author: Prompt Engineering Guide
tags:
  - photo
  - dalle-2
  - art
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, 3D art
{{/run}}
```
***
***
{{get "r"}}