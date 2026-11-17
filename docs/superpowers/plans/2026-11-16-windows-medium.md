# Windows medium changes implementation plan

Goal: complete the requested Windows-only icon, overlay and hybrid capture pass.
Scope: this worktree only; preserve existing changes; no commits or publishing.

1. Extend existing 24-unit vector geometry for notification and layer controls;
   retain accents, branding, localized accessible names and shortcut legends.
   Check XAML parsing/resource references and update notification accent QA.
2. Stop preview polling while invisible/minimized; debounce settings writes;
   retain live position updates and explicit save/close flushing. Remove normal
   indicator rotation and skip unchanged native placement. Avoid unchanged
   opacity settings allocation. Check lifecycle contracts and existing WPF QA.
3. Detect the selected monitor's DXGI adapter in the native bridge. Only when
   ownership is known and differs from the OBS adapter, replace incompatible
   forced DXGI with WGC (preflight ready) or OBS Auto. Keep unknown ownership
   and user configuration unchanged; log selection/fallback without frame reads.
   Add portable policy tests covering mismatch, match and unavailable detection.
4. Run available static checks and diff checks. Record absent PowerShell/.NET
   and all pending Windows builds, WPF, capture and performance measurements in
   ROADMAP.md. No claim of hardware validation or resolved black recordings.
