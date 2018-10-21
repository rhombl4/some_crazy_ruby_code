Team shinh

Team member: Shinichiro Hamaji
Language: C++, Ruby
Country: Japan

* How to build

$ make

Note test will be fail due to missing input.

* Programs

- light: Submission which was meant for lightning round (the
         implementation didn't finish in time).
- full: FA solver, which is an improved version of light. Each bot is
        assigned to 3x3 grids.
- line: Mostly failed attempt to solve FA with GFill.
- void: FD solver with Void, which doesn't perform well :(
- gvoid: FD solver with GVoid, which tries to remove rows from the top
         to th bottom.
- vanish: Solve FD by a single GVoid for small problems.
- recon: FR solver, which was not used for the final result.
- merge_fr: Merge results of FA and FD to solve FR.

For each problem, the best record will be maintained by Ruby/shell
scripts such as collect_xxx.rb and batch_merge.rb.
