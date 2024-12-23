---
promptId: 'ligGoldenHour'
name: '🖼️ Generate a Golden Hour Sunlight photo'
description: 'The hour just after sunrise or just before sunset when the natural light is soft and warm. Increases the temperature of generations.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,lighting'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Golden Hour Sunlight
{{/run}}
```
***
***
{{get "r"}}