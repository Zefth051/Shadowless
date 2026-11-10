# Publishing Shadowless on GitHub

Use one GitHub repository named `Shadowless` with the description
**Shadowless - An app for clipping**. Use `windows` as the default branch and
`linux` for the native Qt/Hyprland edition. The Windows branch owns the Windows
CI and release workflows. Build Linux from the instructions on its branch.

From the Windows Git worktree, after authenticating with GitHub CLI:

```sh
gh repo create Zefth051/Shadowless --private --description "Shadowless - An app for clipping" --source . --remote origin
git push -u origin windows linux
gh repo edit Zefth051/Shadowless --default-branch windows
git ls-remote --heads origin
```

Use `--public` in place of `--private` for a public repository. If the
repository already exists, add its URL as the `origin` remote and start at
the push command. The final command should list both platform branches.

Release assets are shared by all branches in a GitHub repository. Select
`windows` when running the Windows release workflow. Give Linux releases
platform-specific tags such as `linux-v0.2.4` and attach Linux assets there.
