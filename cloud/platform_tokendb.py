DEBUG = False

class StringSet:
    def __init__(self):
        self.strings = set()

    def add_new_elements(self, new_strings):
        initial_size = len(self.strings)
        self.strings.update(new_strings)
        final_size = len(self.strings)
        return final_size - initial_size

if DEBUG:

    string_set = StringSet()

    # Add new strings to the set and print the number of new elements added
    new_strings = ['hello', 'world', 'foo', 'bar']
    print(string_set.add_new_elements(new_strings))  # Output: 4

    # Add some new strings and some existing strings
    new_strings = ['hello', 'world', 'python', 'example']
    print(string_set.add_new_elements(new_strings))  # Output: 2

