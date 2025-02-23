

# GDExtensions plugin

Plugin to display a window above entities like in the Epic City Sample:

![](mass_041.png)

Works if the entity contains an FTransformFragment fragment, all other fragments are optional.

## Installation

In an existing project:

```
cd Plugins (if this directory does not exist create it)
git add Plugins (if you just made it)
git submodule add https://github.com/JohnJFarrow/GDMassExtensions GDMassExtensions
```

In the root project directory

```
git add .gitmodules
```

## Updates

To pull the submodule from each installed project:

```
git submodule update --remote --merge
```