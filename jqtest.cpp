#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_set>

struct Path
{
    int last_place;
    std::unordered_set<int> visited;
    Path(int last, std::unordered_set<int> v) : last_place(last), visited(v) {}
};

int main()
{
    int n = 19;
    std::vector<Path *> pvec;
    pvec.reserve(1000000000);
    Path first_step(100, std::unordered_set<int>{0, 100});
    pvec.push_back(&first_step);

    for (int i = 1; i < n; i++)
    {
        int size = pvec.size();
        for (int j = 0; j < size; ++j)
        {
            Path *p = pvec[j];
            int last_place = p->last_place;

            int x = last_place / 100;
            int y = last_place % 100;

            // up
            if (y + 1 <= n)
            {
                int next_step = x * 100 + (y + 1);

                if (p->visited.find(next_step) == p->visited.end())
                {
                    Path *new_path = new Path(*p);
                    new_path->last_place = next_step;
                    new_path->visited.insert(next_step);
                    pvec.push_back(new_path);
                }
            }

            // down
            if (y - 1 >= 0)
            {
                int next_step = x * 100 + (y - 1);

                if (p->visited.find(next_step) == p->visited.end())
                {
                    Path *new_path = new Path(*p);
                    new_path->last_place = next_step;
                    new_path->visited.insert(next_step);
                    pvec.push_back(new_path);
                }
            }

            // left
            if (x - 1 >= 0)
            {
                int next_step = (x - 1) * 100 + y;

                if (p->visited.find(next_step) == p->visited.end())
                {
                    Path *new_path = new Path(*p);
                    new_path->last_place = next_step;
                    new_path->visited.insert(next_step);
                    pvec.push_back(new_path);
                }
            }
            // right
            if (x + 1 <= n)
            {
                int next_step = (x + 1) * 100 + y;

                if (p->visited.find(next_step) == p->visited.end())
                {
                    Path *new_path = new Path(*p);
                    new_path->last_place = next_step;
                    new_path->visited.insert(next_step);
                    pvec.push_back(new_path);
                }
            }

            pvec[j]=pvec.back();
            pvec.pop_back();
        }
    }

    std::cout << "Final size of pset: " << pvec.size() << std::endl;
    return 0;
}