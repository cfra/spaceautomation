import colorsys
import random

class ColorGen():
    def __init__(self, seed=0):
        self.random = random.Random()
        self.random.seed(seed)
        self.color_list = [(1.0,1.0,1.0)]

    def rate(self, color):
        distances = []
        for other_color in self.color_list:
            distances.append(self.distance(color, other_color))
        return self.get_score(distances)

    def get_score(self, distances):
        return min(distances)

    def distance(self, a, b):
        return ((a[0]-b[0])**2 + (a[1]-b[1])**2 + (a[2]-b[2])**2) ** 0.5

    def mutate(self, candidate):
        rv = [0,0,0]
        for i in range(0,2):
            rv[i] = candidate[i] + self.random.uniform(-0.1,0.1)
            if rv[i] < 0:
                rv[i] = 0.0
            if rv[i] > 1:
                rv[i] = 1.0
        return tuple(rv)

    def get_next(self):
        candidates = []
        for i in range(0, 50): # Create 50 candidates
            candidates.append((
                    self.random.uniform(0,1),
                    self.random.uniform(0,1),
                    self.random.uniform(0,1)
            ))

        for r in range(0, 40): # Do 40 Rounds
            for c in list(candidates):
                for i in range(0,5): # Create 5 decendants of each candidate
                    candidates.append(self.mutate(c))
            # Keep the best ten candidates
            candidates.sort(key=lambda candidate:-self.rate(candidate))
            candidates = candidates[:10]

        # And the winner is:
        self.color_list.append(candidates[0])
        return candidates[0]
