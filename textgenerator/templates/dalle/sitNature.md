---
promptId: 'sitNature'
name: '🖼️ Generate a Nature photo'
description: 'Photographs in the dataset with these captions tend to showcase animals/nature in extraordinary positions and situations, works similarly to “Award-Winning” but is only for nature. This will also make animals/nature look more real and accurate.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2, situational'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Nature Photography
{{/run}}
```
***
***
{{get "r"}}