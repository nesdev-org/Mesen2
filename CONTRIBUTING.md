# Licensing

Although MesenCE is currently GPL, we are interested in removing GPL code over time and potentially relicensing the emulator under a more permissive license, such as MIT, at some point in the future. Thus, if you submit code to MesenCE, you agree that:
- **Your code is licensed under the MIT License.** You may explicitly provide it under additional optional licenses if you wish.
- Your submission does not contain code that is incompatible with the MIT License. (For example, your submission may not be based on GPL code or include a GPL library.)

# AI / LLM code

**AI-generated code is prohibited.** While you may consult with LLMs, all submitted code must be written and understood by you, the contributor, to ensure accuracy and copyright integrity.

# Style

MesenCE requires a consistent style across the codebase. Most of this style is currently enforced with clang-format (for C++) and dotnet format (for C#), and these must be run on any code submitted to MesenCE. In Visual Studio, these can be configured to run automatically when saving. On other platforms, you can use these commands:

- clang-format: `find ./ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i`
- dotnet format: `dotnet format`

In addition, we require the following naming conventions:

- ExampleFunction
- exampleVariable
- _exampleMemberVariable

When in doubt, follow the formatting you see elsewhere in the project.

# Guidance

- Performance: Changes may be rejected if they cause a drop in maximum FPS (press F9 to run at an unlocked framerate and F10 to view the FPS). Where possible, ensure that your changes avoid doing things like adding if statements to hot paths.
- Warnings: The MesenCE MSVC builds treat warnings as errors. These must be resolved before code can be accepted.
- Commit messages: Including context and test ROMs with your pull requests allows us to more easily and quickly evaluate the code. If we don't understand a change, it is less likely to be accepted.

## Known pull requests that WON'T be accepted
- Translations
- Radical changes to the emulated systems (different audio mixer, unit-specific differences, etc.)
- Support for different platforms

Basically, if your change is too radical, or too niche, it may not get accepted. As big as it may be, MesenCE is not intended to cover every niche use case, as this requires a lot of time the maintainers may not have, and the result may turn out not to be as useful.

**Pull requests may be rejected for a variety of other reasons, even if they are functional. Please consult with the team before making any significant changes.**
